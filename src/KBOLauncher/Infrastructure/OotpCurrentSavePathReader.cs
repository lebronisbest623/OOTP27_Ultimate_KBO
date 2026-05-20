using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

internal static partial class OotpCurrentSavePathReader
{
    public static CurrentSavePathInfo TryRead(int pid, Action<string>? log = null)
    {
        using var process = Process.GetProcessById(pid);
        ProcessModule? mainModule;
        try
        {
            mainModule = process.MainModule;
        }
        catch (Exception ex) when (ex is Win32Exception or InvalidOperationException)
        {
            return CurrentSavePathInfo.Fail("process_module_unreadable", null, $"{ex.GetType().Name}: {ex.Message}");
        }

        if (mainModule is null || string.IsNullOrWhiteSpace(mainModule.FileName))
        {
            return CurrentSavePathInfo.Fail("process_module_missing", null, "main module was not available");
        }

        var processHandle = NativeMethods.OpenProcess(
            NativeMethods.ProcessAccessFlags.QueryInformation | NativeMethods.ProcessAccessFlags.VirtualMemoryRead,
            false,
            pid);
        if (processHandle == IntPtr.Zero)
        {
            return CurrentSavePathInfo.Fail("open_process_failed", null, new Win32Exception(Marshal.GetLastWin32Error()).Message);
        }

        try
        {
            var sections = ReadWritableDataSections(mainModule.FileName);
            if (sections.Count == 0)
            {
                return CurrentSavePathInfo.Fail("pe_sections_missing", null, "no writable PE sections found");
            }

            var baseAddress = unchecked((ulong)mainModule.BaseAddress.ToInt64());
            foreach (var section in sections)
            {
                var sectionAddress = baseAddress + section.VirtualAddress;
                var sectionBytes = ReadMemory(processHandle, sectionAddress, checked((int)section.VirtualSize));
                if (sectionBytes is null)
                {
                    continue;
                }

                for (var offset = 0; offset + 8 <= sectionBytes.Length; offset += 8)
                {
                    var candidate = BitConverter.ToUInt64(sectionBytes, offset);
                    if (candidate < Win32ProbeConstants.MinUserPointer || candidate > Win32ProbeConstants.MaxUserPointer)
                    {
                        continue;
                    }
                    if (!LooksLikeGlobalDatabase(processHandle, candidate))
                    {
                        continue;
                    }

                    var savePath = ReadOotpString(processHandle, candidate + OotpAbiOffsets.MessageSavePath, 512);
                        log?.Invoke($"current_save_probe pid={pid} global=0x{candidate:X} save=\"{savePath ?? ""}\"");
                        if (LooksLikeAbsoluteLgSavePath(savePath))
                        {
                            return new CurrentSavePathInfo(true, "current_save_found", Path.GetFullPath(savePath!), null);
                        }
                }
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException or OverflowException)
        {
            return CurrentSavePathInfo.Fail("current_save_probe_failed", null, $"{ex.GetType().Name}: {ex.Message}");
        }
        finally
        {
            NativeMethods.CloseHandle(processHandle);
        }

        var handleSavePath = TryReadFromOpenFileHandles(pid, log);
        if (LooksLikeAbsoluteLgSavePath(handleSavePath))
        {
            return new CurrentSavePathInfo(true, "current_save_found_from_handles", Path.GetFullPath(handleSavePath!), null);
        }

        return CurrentSavePathInfo.Fail("current_save_unavailable", null, $"could not read an opened {OotpProduct.SaveGameExtension} save path from the target process");
    }

    private static List<PeSection> ReadWritableDataSections(string exePath)
    {
        using var stream = File.Open(exePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        using var reader = new BinaryReader(stream);

        if (stream.Length < PeImageConstants.DosHeaderMinBytes)
        {
            return [];
        }

        stream.Position = PeImageConstants.PeHeaderPointerOffset;
        var peHeaderOffset = reader.ReadInt32();
        if (peHeaderOffset <= 0 || peHeaderOffset + PeImageConstants.CoffHeaderBytes > stream.Length)
        {
            return [];
        }

        stream.Position = peHeaderOffset;
        if (reader.ReadUInt32() != PeImageConstants.PeSignature)
        {
            return [];
        }

        stream.Position = peHeaderOffset + 6;
        var sectionCount = reader.ReadUInt16();
        stream.Position = peHeaderOffset + 20;
        var optionalHeaderSize = reader.ReadUInt16();

        var sectionOffset = peHeaderOffset + PeImageConstants.CoffHeaderBytes + optionalHeaderSize;
        var sections = new List<PeSection>();
        for (var i = 0; i < sectionCount; i++)
        {
            var current = sectionOffset + i * PeImageConstants.SectionHeaderBytes;
            if (current + PeImageConstants.SectionHeaderBytes > stream.Length)
            {
                break;
            }

            stream.Position = current + 8;
            var virtualSize = reader.ReadUInt32();
            var virtualAddress = reader.ReadUInt32();
            stream.Position = current + 36;
            var characteristics = reader.ReadUInt32();

            var writableData = (characteristics & PeImageConstants.SectionMemoryWrite) != 0
                && (characteristics & PeImageConstants.SectionMemoryRead) != 0
                && (characteristics & PeImageConstants.SectionMemoryExecute) == 0;
            if (writableData && virtualSize >= 8)
            {
                sections.Add(new PeSection(virtualAddress, virtualSize));
            }
        }

        return sections;
    }

    private static bool LooksLikeGlobalDatabase(IntPtr processHandle, ulong candidate)
    {
        var header = ReadMemory(processHandle, candidate, 0xB0);
        if (header is null)
        {
            return false;
        }

        var teamCount = BitConverter.ToInt32(header, (int)OotpAbiOffsets.TeamCount);
        if (teamCount < 2 || teamCount > 500)
        {
            return false;
        }

        var teamVector = BitConverter.ToUInt64(header, (int)OotpAbiOffsets.TeamVector);
        if (teamVector == 0)
        {
            return false;
        }

        var teamPointers = ReadMemory(processHandle, teamVector, teamCount * 8);
        if (teamPointers is null)
        {
            return false;
        }

        var checks = Math.Min(teamCount, 5);
        for (var i = 0; i < checks; i++)
        {
            var team = BitConverter.ToUInt64(teamPointers, i * 8);
            if (team == 0)
            {
                return false;
            }

            var teamIdBytes = ReadMemory(processHandle, team + OotpAbiOffsets.TeamId, 4);
            var leagueIdBytes = ReadMemory(processHandle, team + OotpAbiOffsets.TeamLeagueId, 4);
            if (teamIdBytes is null || leagueIdBytes is null)
            {
                return false;
            }

            var teamId = BitConverter.ToUInt32(teamIdBytes, 0);
            var leagueId = BitConverter.ToUInt32(leagueIdBytes, 0);
            if (teamId == 0 || teamId > 100000 || leagueId == 0 || leagueId > 100000)
            {
                return false;
            }
        }

        return true;
    }

    private static string? ReadOotpString(IntPtr processHandle, ulong stringObjectAddress, int maxBytes)
    {
        var pointerBytes = ReadMemory(processHandle, stringObjectAddress + OotpAbiOffsets.StringObjectText, 8);
        if (pointerBytes is null)
        {
            return null;
        }

        var textPointer = BitConverter.ToUInt64(pointerBytes, 0);
        if (textPointer < Win32ProbeConstants.MinUserPointer)
        {
            return null;
        }

        var bytes = ReadMemory(processHandle, textPointer, maxBytes);
        if (bytes is null)
        {
            return null;
        }

        return DecodeNullTerminatedOotpPathString(bytes);
    }

    internal static string? DecodeNullTerminatedOotpPathString(byte[] bytes)
    {
        var used = 0;
        for (; used < bytes.Length; used++)
        {
            if (bytes[used] == 0)
            {
                break;
            }
        }

        if (used == 0)
        {
            return null;
        }

        var utf8 = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true);
        string text;
        try
        {
            text = utf8.GetString(bytes, 0, used);
        }
        catch (DecoderFallbackException)
        {
            text = Encoding.Default.GetString(bytes, 0, used);
        }

        return text.Any(ch => char.IsControl(ch) && ch is not '\t')
            ? null
            : text;
    }

    private static byte[]? ReadMemory(IntPtr processHandle, ulong address, int size)
    {
        if (size <= 0 || address < Win32ProbeConstants.MinUserPointer)
        {
            return null;
        }

        var buffer = new byte[size];
        if (!NativeMethods.ReadProcessMemory(
                processHandle,
                unchecked((IntPtr)(long)address),
                buffer,
                size,
                out var bytesRead)
            || bytesRead.ToInt64() != size)
        {
            return null;
        }

        return buffer;
    }

    private readonly record struct PeSection(uint VirtualAddress, uint VirtualSize);
}
