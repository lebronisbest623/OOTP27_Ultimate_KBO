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
    try
    {
        var logDir = OotpProduct.LocalDataDirectory;
        Directory.CreateDirectory(logDir);
        File.AppendAllText(
            Path.Combine(logDir, OotpProduct.LauncherLogFileName),
            $"{DateTimeOffset.Now:yyyy-MM-dd HH:mm:ss.fff zzz} fatal {ex.GetType().Name}: {ex.Message}{Environment.NewLine}{ex}{Environment.NewLine}");
    }
    catch
    {
    }

    Console.Error.WriteLine("Launcher failed:");
    Console.Error.WriteLine(ex);
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
