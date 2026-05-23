try
{
    var exitCode = LauncherApp.Run(args);
    if (exitCode != 0)
    {
        PauseBeforeExit();
    }
    return exitCode;
}
catch (Exception ex)
{
    string? diagnosticsPath = null;
    try
    {
        var logDir = OotpProduct.LocalDataDirectory;
        Directory.CreateDirectory(logDir);
        File.AppendAllText(
            Path.Combine(logDir, OotpProduct.LauncherLogFileName),
            $"{DateTimeOffset.Now:yyyy-MM-dd HH:mm:ss.fff zzz} fatal {ex.GetType().Name}: {ex.Message}{Environment.NewLine}{ex}{Environment.NewLine}");
        diagnosticsPath = DiagnosticBundle.CreateForFatal(ex);
    }
    catch
    {
    }

    Console.Error.WriteLine("Launcher failed:");
    Console.Error.WriteLine(ex);
    if (!string.IsNullOrWhiteSpace(diagnosticsPath))
    {
        Console.Error.WriteLine();
        Console.Error.WriteLine($"Diagnostics bundle written to: {diagnosticsPath}");
        Console.Error.WriteLine("Open ACTION_REQUIRED.txt inside the ZIP first. If it still fails, send the whole ZIP with your bug report.");
    }
    PauseBeforeExit();
    return 99;
}

static void PauseBeforeExit()
{
    if (!Environment.UserInteractive || Console.IsInputRedirected)
    {
        return;
    }

    Console.Error.WriteLine();
    Console.Error.WriteLine("Press Enter to close this window...");
    try
    {
        Console.ReadLine();
    }
    catch
    {
    }
}
