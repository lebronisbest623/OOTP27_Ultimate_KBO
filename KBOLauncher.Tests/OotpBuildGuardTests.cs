namespace KBOLauncher.Tests;

using FluentAssertions;
using Xunit;

public sealed class OotpBuildGuardTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-launcher-build-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void Read_RejectsTooSmallFile()
    {
        var path = Path.Combine(tempDir, "tiny.exe");
        Directory.CreateDirectory(tempDir);
        File.WriteAllBytes(path, [0x4D, 0x5A]);

        var info = global::OotpBuildGuard.Read(path);

        info.Ok.Should().BeFalse();
        info.Error.Should().Be("file too small");
        global::OotpBuildGuard.FindSupportedBuild(info).Should().BeNull();
    }

    [Fact]
    public void Read_ParsesPeTimestampAndImageSize()
    {
        var path = Path.Combine(tempDir, "ootp27.exe");
        WriteMinimalPe(path, 0x12345678u, 0x00100000u);

        var info = global::OotpBuildGuard.Read(path);

        info.Ok.Should().BeTrue();
        info.Timestamp.Should().Be(0x12345678u);
        info.SizeOfImage.Should().Be(0x00100000u);
        global::OotpBuildGuard.FindSupportedBuild(info).Should().BeNull();
        global::OotpBuildGuard.FormatConsoleStatus(info, null).Should().Contain("unsupported");
    }

    [Fact]
    public void Read_RejectsInvalidPeSignaturesAndHeaderOffsets()
    {
        var invalidDos = Path.Combine(tempDir, "invalid-dos.exe");
        Directory.CreateDirectory(tempDir);
        File.WriteAllBytes(invalidDos, new byte[0x200]);
        global::OotpBuildGuard.Read(invalidDos).Error.Should().Be("invalid DOS signature");

        var invalidOffset = Path.Combine(tempDir, "invalid-offset.exe");
        var offsetBytes = new byte[0x200];
        offsetBytes[0] = 0x4D;
        offsetBytes[1] = 0x5A;
        BitConverter.GetBytes(0x1FF).CopyTo(offsetBytes, 0x3C);
        File.WriteAllBytes(invalidOffset, offsetBytes);
        global::OotpBuildGuard.Read(invalidOffset).Error.Should().Be("invalid PE header offset");

        var invalidPe = Path.Combine(tempDir, "invalid-pe.exe");
        WriteMinimalPe(invalidPe, 0x12345678u, 0x00100000u);
        using (var stream = File.OpenWrite(invalidPe))
        {
            stream.Position = 0x80;
            stream.WriteByte(0x00);
        }

        global::OotpBuildGuard.Read(invalidPe).Error.Should().Be("invalid PE signature");
    }

    [Fact]
    public void FindSupportedBuild_MatchesNativePatchSupportedBuilds()
    {
        foreach (var expected in global::OotpSupportedBuilds.All.Where(build => build.NativePatchesSupported))
        {
            var info = new global::OotpBuildInfo(true, expected.Timestamp, expected.SizeOfImage, null);

            var supported = global::OotpBuildGuard.FindSupportedBuild(info);

            supported.Should().NotBeNull();
            supported!.Label.Should().Be(expected.Label);
            global::OotpBuildGuard.FormatConsoleStatus(info, supported).Should().Contain($"supported ({expected.Label})");
            global::OotpBuildGuard.FormatLogStatus(info, supported).Should().Contain("status=supported");
            global::OotpBuildGuard.SupportedBuildDescriptions().Should().Contain(
                $"{expected.Label}:0x{expected.Timestamp:X8}/0x{expected.SizeOfImage:X8}" +
                (expected.ExperimentalSignature ? "[experimental_signature]" : "") +
                "[native_patches]");
        }
    }

    [Fact]
    public void FindKnownBuild_MatchesMetadataOnlyBuildsWithoutNativeSupport()
    {
        foreach (var expected in global::OotpSupportedBuilds.All.Where(build => !build.NativePatchesSupported))
        {
            var info = new global::OotpBuildInfo(true, expected.Timestamp, expected.SizeOfImage, null);

            global::OotpBuildGuard.FindSupportedBuild(info).Should().BeNull();
            var known = global::OotpBuildGuard.FindKnownBuild(info);

            known.Should().NotBeNull();
            known!.Label.Should().Be(expected.Label);
            global::OotpBuildGuard.FormatConsoleStatus(info, known).Should().Contain($"metadata-only ({expected.Label})");
            global::OotpBuildGuard.FormatLogStatus(info, known).Should().Contain("status=metadata_only");
            global::OotpBuildGuard.SupportedBuildDescriptions().Should().Contain(
                $"{expected.Label}:0x{expected.Timestamp:X8}/0x{expected.SizeOfImage:X8}" +
                (expected.ExperimentalSignature ? "[experimental_signature]" : "") +
                "[metadata_only]");
        }
    }

    [Fact]
    public void FindSupportedBuild_RejectsUnknownBuild()
    {
        var info = new global::OotpBuildInfo(true, 0x11111111u, 0x02222222u, null);

        var supported = global::OotpBuildGuard.FindSupportedBuild(info);
        var known = global::OotpBuildGuard.FindKnownBuild(info);

        supported.Should().BeNull();
        known.Should().BeNull();
        global::OotpBuildGuard.FormatConsoleStatus(info, supported).Should().Contain("unsupported");
        global::OotpBuildGuard.SupportedBuildDescriptions().Should().NotContain("Official");
    }

    [Fact]
    public void FormatStatuses_IncludeUnreadableAndUnsupportedDetails()
    {
        var unreadable = global::OotpBuildInfo.Fail("boom");
        var unsupported = new global::OotpBuildInfo(true, 0x11111111u, 0x22222222u, null);

        global::OotpBuildGuard.FormatConsoleStatus(unreadable, null).Should().Be("OOTP build: unreadable (boom)");
        global::OotpBuildGuard.FormatLogStatus(unreadable, null).Should().Be("ootp_build status=unreadable error=\"boom\"");
        global::OotpBuildGuard.FormatConsoleStatus(unsupported, null).Should().Contain("timestamp=0x11111111");
        global::OotpBuildGuard.FormatLogStatus(unsupported, null).Should().Be(
            "ootp_build status=unsupported timestamp=0x11111111 size_of_image=0x22222222");
    }

    private static void WriteMinimalPe(string path, uint timestamp, uint sizeOfImage)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var bytes = new byte[0x200];
        bytes[0] = 0x4D;
        bytes[1] = 0x5A;
        BitConverter.GetBytes(0x80).CopyTo(bytes, 0x3C);
        bytes[0x80] = 0x50;
        bytes[0x81] = 0x45;
        bytes[0x82] = 0x00;
        bytes[0x83] = 0x00;
        BitConverter.GetBytes(timestamp).CopyTo(bytes, 0x80 + 8);
        BitConverter.GetBytes(sizeOfImage).CopyTo(bytes, 0x80 + 24 + 0x38);
        File.WriteAllBytes(path, bytes);
    }

    public void Dispose()
    {
        try
        {
            if (Directory.Exists(tempDir))
            {
                Directory.Delete(tempDir, recursive: true);
            }
        }
        catch (IOException)
        {
        }
    }
}
