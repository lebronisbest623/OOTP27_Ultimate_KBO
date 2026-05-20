using System.CommandLine;

record LauncherOptions(
    string? OotpPath,
    string? DllPath,
    int? AttachPid,
    bool AttachExisting,
    bool? EnableForeignWaiverAi,
    bool? EnableSingleDivisionAllstarEvents,
    bool DryRun,
    bool AllowSecondInstance,
    bool ShowHelp,
    IReadOnlyList<string> OotpArgs)
{
    private static readonly Option<string?> OotpPathOpt =
        new("--ootp", $"Explicit {OotpProduct.ExecutableFileName} path.");
    private static readonly Option<string?> DllPathOpt =
        new("--dll", "Native DLL to load into OOTP.");
    private static readonly Option<int?> AttachPidOpt =
        new("--attach-pid", "Attach to a specific OOTP process id.");
    private static readonly Option<bool> AttachExistingOpt =
        new("--attach-existing", "Attach to the single running OOTP process.");
    private static readonly Option<bool> EnableForeignWaiverAiOpt =
        new("--enable-foreign-waiver-ai", "Enable foreign waiver assistant log.");
    private static readonly Option<bool> DisableForeignWaiverAiOpt =
        new("--disable-foreign-waiver-ai", "Disable foreign waiver assistant log.");
    private static readonly Option<bool> EnableAllstarEventsOpt =
        new("--enable-single-division-allstar-events", "Enable single-division all-star event creation.");
    private static readonly Option<bool> DisableAllstarEventsOpt =
        new("--disable-single-division-allstar-events", "Disable single-division all-star event creation.");
    private static readonly Option<bool> DryRunOpt =
        new("--dry-run", "Resolve paths and detect existing process without launching.");
    private static readonly Option<bool> AllowSecondInstanceOpt =
        new("--allow-second-instance", "Launch even if an OOTP process is already running.");

    private static readonly RootCommand Command = BuildCommand();

    private static RootCommand BuildCommand()
    {
        var cmd = new RootCommand($"KBOLauncher — {OotpProduct.ProductShortFolderName} KBO mod launcher");
        cmd.AddOption(OotpPathOpt);
        cmd.AddOption(DllPathOpt);
        cmd.AddOption(AttachPidOpt);
        cmd.AddOption(AttachExistingOpt);
        cmd.AddOption(EnableForeignWaiverAiOpt);
        cmd.AddOption(DisableForeignWaiverAiOpt);
        cmd.AddOption(EnableAllstarEventsOpt);
        cmd.AddOption(DisableAllstarEventsOpt);
        cmd.AddOption(DryRunOpt);
        cmd.AddOption(AllowSecondInstanceOpt);
        return cmd;
    }

    public static LauncherOptions Parse(string[] args)
    {
        var showHelp = args.Any(a => a is "-h" or "--help");

        // Split at -- so OOTP passthrough args are not parsed as launcher options
        var dashDashIdx = Array.IndexOf(args, "--");
        string[] launcherArgs;
        List<string> ootpArgs;
        if (dashDashIdx >= 0)
        {
            launcherArgs = args[..dashDashIdx];
            ootpArgs = [..args[(dashDashIdx + 1)..]];
        }
        else
        {
            launcherArgs = args;
            ootpArgs = [];
        }

        // Strip --help/-h before SC parsing — we handle help output ourselves
        launcherArgs = launcherArgs.Where(a => a is not ("-h" or "--help")).ToArray();

        var parseResult = Command.Parse(launcherArgs);
        if (parseResult.Errors.Count > 0)
        {
            throw new ArgumentException(parseResult.Errors[0].Message);
        }

        bool? enableForeignWaiverAi = null;
        if (parseResult.GetValueForOption(EnableForeignWaiverAiOpt)) enableForeignWaiverAi = true;
        if (parseResult.GetValueForOption(DisableForeignWaiverAiOpt)) enableForeignWaiverAi = false;

        bool? enableSingleDivisionAllstarEvents = null;
        if (parseResult.GetValueForOption(EnableAllstarEventsOpt)) enableSingleDivisionAllstarEvents = true;
        if (parseResult.GetValueForOption(DisableAllstarEventsOpt)) enableSingleDivisionAllstarEvents = false;

        return new LauncherOptions(
            parseResult.GetValueForOption(OotpPathOpt),
            parseResult.GetValueForOption(DllPathOpt),
            parseResult.GetValueForOption(AttachPidOpt),
            parseResult.GetValueForOption(AttachExistingOpt),
            enableForeignWaiverAi,
            enableSingleDivisionAllstarEvents,
            parseResult.GetValueForOption(DryRunOpt),
            parseResult.GetValueForOption(AllowSecondInstanceOpt),
            showHelp,
            ootpArgs);
    }

    public static void PrintHelp()
    {
        Console.WriteLine("""
        KBOLauncher

        Usage:
          KBOLauncher
          KBOLauncher [--ootp path] [--allow-second-instance] [--] [ootp args...]
          KBOLauncher --attach-existing --dll path
          KBOLauncher --attach-pid pid --dll path

        Options:
          --ootp PATH                  Explicit {0} path.
          --dll PATH                   Native DLL to load into OOTP.
          --attach-existing            Attach to the single running OOTP process.
          --attach-pid PID             Attach to a specific OOTP process id.
          --enable-foreign-waiver-ai    Enable foreign waiver assistant log.
          --disable-foreign-waiver-ai   Disable foreign waiver assistant log.
          --enable-single-division-allstar-events
                                       Enable single-division all-star event creation.
          --disable-single-division-allstar-events
                                       Disable single-division all-star event creation.
          --dry-run                    Resolve paths and detect existing process without launching.
          --allow-second-instance      Launch even if an OOTP process is already running.
          --                           Pass remaining arguments to OOTP.

        Safety:
          KBOFix injection is disabled unless {0} matches a verified build.
          KBOFix injection also requires the currently opened OOTP {1} save {2}
          to contain the official roster marker URL and a completed save flag.
        """, OotpProduct.ExecutableFileName, OotpProduct.SaveGameExtension, OotpProduct.DescriptionFileName);
    }
}
