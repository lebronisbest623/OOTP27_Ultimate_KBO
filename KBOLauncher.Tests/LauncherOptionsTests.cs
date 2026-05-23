namespace KBOLauncher.Tests;

using FluentAssertions;
using Xunit;

public sealed class LauncherOptionsTests
{
    [Fact]
    public void Parse_RecognizesLauncherFlagsAndOotpArguments()
    {
        var options = global::LauncherOptions.Parse([
            "--ootp", @"D:\Games\ootp27.exe",
            "--dll", @"C:\mods\KBOFix.dll",
            "--attach-existing",
            "--disable-foreign-waiver-ai",
            "--",
            "no3d",
            "debug"
        ]);

        options.OotpPath.Should().Be(@"D:\Games\ootp27.exe");
        options.DllPath.Should().Be(@"C:\mods\KBOFix.dll");
        options.AttachExisting.Should().BeTrue();
        options.EnableForeignWaiverAi.Should().BeFalse();
        options.OotpArgs.Should().Equal("no3d", "debug");
    }

    [Fact]
    public void Parse_RejectsAttachPidWithoutNumericValue()
    {
        var act = () => global::LauncherOptions.Parse(["--attach-pid", "abc"]);
        act.Should().Throw<ArgumentException>().WithMessage("*--attach-pid*");
    }

    [Fact]
    public void Parse_RecognizesDisableFlagsDryRunAndHelp()
    {
        var options = global::LauncherOptions.Parse([
            "--attach-pid", "1234",
            "--enable-foreign-waiver-ai",
            "--disable-single-division-allstar-events",
            "--dry-run",
            "--allow-second-instance",
            "--diagnostics",
            "--help"
        ]);

        options.AttachPid.Should().Be(1234);
        options.EnableForeignWaiverAi.Should().BeTrue();
        options.EnableSingleDivisionAllstarEvents.Should().BeFalse();
        options.DryRun.Should().BeTrue();
        options.AllowSecondInstance.Should().BeTrue();
        options.Diagnostics.Should().BeTrue();
        options.ShowHelp.Should().BeTrue();
    }

    [Fact]
    public void Parse_RejectsUnknownArgument()
    {
        var act = () => global::LauncherOptions.Parse(["--wat"]);
        act.Should().Throw<ArgumentException>().WithMessage("*--wat*");
    }
}
