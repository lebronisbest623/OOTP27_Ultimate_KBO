using System.ComponentModel;
using System.Runtime.InteropServices;

internal static partial class OotpCurrentSavePathReader
{
    private static string? TryReadFromOpenFileHandles(int pid, Action<string>? log)
    {
        var queryLength = Win32ProbeConstants.InitialHandleQueryBytes;
        byte[] buffer;
        int status;
        int returnLength;
        do
        {
            buffer = new byte[queryLength];
            status = NativeMethods.NtQuerySystemInformation(
                Win32ProbeConstants.SystemExtendedHandleInformation,
                buffer,
                buffer.Length,
                out returnLength);
            if (status == Win32ProbeConstants.NtStatusInfoLengthMismatch && returnLength > queryLength)
            {
                queryLength = returnLength + Win32ProbeConstants.InitialHandleQueryBytes;
            }
            else if (status == Win32ProbeConstants.NtStatusInfoLengthMismatch)
            {
                queryLength *= 2;
            }
        } while (status == Win32ProbeConstants.NtStatusInfoLengthMismatch && queryLength <= Win32ProbeConstants.MaxHandleQueryBytes);

        if (status != 0)
        {
            log?.Invoke($"current_save_handle_probe_failed pid={pid} status=0x{status:X8}");
            return null;
        }

        var sourceProcess = NativeMethods.OpenProcess(
            NativeMethods.ProcessAccessFlags.DuplicateHandle | NativeMethods.ProcessAccessFlags.QueryInformation,
            false,
            pid);
        if (sourceProcess == IntPtr.Zero)
        {
            log?.Invoke($"current_save_handle_probe_open_failed pid={pid} error=\"{new Win32Exception(Marshal.GetLastWin32Error()).Message}\"");
            return null;
        }

        try
        {
            var currentProcess = NativeMethods.GetCurrentProcess();
            var handleCount = checked((long)(IntPtr.Size == 8
                ? BitConverter.ToUInt64(buffer, 0)
                : BitConverter.ToUInt32(buffer, 0)));
            var offset = IntPtr.Size == 8 ? 16 : 8;
            var entrySize = IntPtr.Size == 8 ? 40 : 28;
            for (var i = 0L; i < handleCount; i++)
            {
                var entryOffset = offset + checked((int)(i * entrySize));
                if (entryOffset + entrySize > buffer.Length)
                {
                    break;
                }

                var ownerPid = IntPtr.Size == 8
                    ? BitConverter.ToUInt64(buffer, entryOffset + 8)
                    : BitConverter.ToUInt32(buffer, entryOffset + 8);
                if (ownerPid != (uint)pid)
                {
                    continue;
                }

                var rawHandle = IntPtr.Size == 8
                    ? unchecked((IntPtr)(long)BitConverter.ToUInt64(buffer, entryOffset + 16))
                    : unchecked((IntPtr)(int)BitConverter.ToUInt32(buffer, entryOffset + 16));
                if (!NativeMethods.DuplicateHandle(
                        sourceProcess,
                        rawHandle,
                        currentProcess,
                        out var duplicatedHandle,
                        0,
                        false,
                        NativeMethods.DuplicateOptions.SameAccess))
                {
                    continue;
                }

                try
                {
                    if (NativeMethods.GetFileType(duplicatedHandle) != NativeMethods.FileTypeDisk)
                    {
                        continue;
                    }

                    var path = GetPathFromHandle(duplicatedHandle);
                    var savePath = ExtractLgSavePath(path);
                    if (LooksLikeAbsoluteLgSavePath(savePath))
                    {
                        log?.Invoke($"current_save_handle_probe pid={pid} save=\"{savePath}\" source=\"{path}\"");
                        return savePath;
                    }
                }
                finally
                {
                    NativeMethods.CloseHandle(duplicatedHandle);
                }
            }
        }
        finally
        {
            NativeMethods.CloseHandle(sourceProcess);
        }

        log?.Invoke($"current_save_handle_probe_unavailable pid={pid}");
        return null;
    }


    private static string? GetPathFromHandle(IntPtr handle)
    {
        var buffer = new char[1024];
        var length = NativeMethods.GetFinalPathNameByHandle(handle, buffer, buffer.Length, 0);
        if (length == 0)
        {
            return null;
        }
        if (length >= buffer.Length)
        {
            buffer = new char[length + 1];
            length = NativeMethods.GetFinalPathNameByHandle(handle, buffer, buffer.Length, 0);
            if (length == 0 || length >= buffer.Length)
            {
                return null;
            }
        }
        return new string(buffer, 0, checked((int)length));
    }


    internal static string? ExtractLgSavePath(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return null;
        }

        var normalized = path.StartsWith(@"\\?\", StringComparison.Ordinal)
            ? path[4..]
            : path;
        var marker = OotpProduct.SaveGameExtension;
        var index = normalized.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
        if (index < 0)
        {
            return null;
        }

        var end = index + marker.Length;
        if (end < normalized.Length && normalized[end] != '\\' && normalized[end] != '/')
        {
            return null;
        }

        return normalized[..end];
    }


    internal static bool LooksLikeAbsoluteLgSavePath(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return false;
        }

        var absolute = Path.IsPathFullyQualified(path);
        return absolute
            && path.EndsWith(OotpProduct.SaveGameExtension, StringComparison.OrdinalIgnoreCase)
            && Directory.Exists(path);
    }

}
