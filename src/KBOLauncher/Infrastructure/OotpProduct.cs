internal static class OotpProduct
{
    public const string ExecutableFileName = "ootp27.exe";
    public const string LocalDataDirectoryName = "OOTP-KBO";
    public const string VendorFolderName = "Out of the Park Developments";
    public const string ProductFolderName = "OOTP Baseball 27";
    public const string ProductLongFolderName = "Out of the Park Baseball 27";
    public const string ProductShortFolderName = "OOTP 27";
    public const string SteamDirectoryName = "Steam";
    public const string SteamAppsDirectoryName = "steamapps";
    public const string SteamCommonDirectoryName = "common";
    public const string SavedGamesDirectoryName = "saved_games";
    public const string SchedulesDirectoryName = "schedules";
    public const string KboScheduleSearchPattern = "korean_baseball_organization_int_c_*.lsdl";
    public const string DataDirectoryName = "data";
    public const string SaveGameExtension = ".lg";
    public const string SaveGameSearchPattern = "*.lg";
    public const string RosterMarkerUrl = "https://github.com/lebronisbest623/OOTP27_Ultimate_KBO";
    public const string DescriptionFileName = "description.txt";
    public const string SaveStartedFileName = "flag_save_started.dat";
    public const string SaveCompletedFileName = "flag_save_completed.dat";
    public const string SaveCompletedSentinel = "Finished save_database, closing flag file now";
    public const string LeagueIdFileName = "kbo_league_id.txt";
    public const string FlagsFileName = "kbo_flags.json";
    public const string SettingsFileName = "kbo_settings.json";
    public const string SaveStateSqliteFileName = "kbo_state.sqlite3";
    public const string LauncherLogFileName = "launcher.log";
    public const string PathDiscoveryStatusFileName = "launcher_path_discovery_status.txt";
    public const string RosterMarkerGuardStatusFileName = "launcher_roster_marker_guard_status.txt";
    public const string CurrentSavePathFileTemplate = "current_save_path_{0}.txt";

    public static readonly string[] OotpEnvironmentVariables =
    [
        "OOTP27_EXE",
        "OOTP27_DIR",
        "OOTP_DIR",
    ];

    public static readonly string[] ProductFolderNames =
    [
        ProductFolderName,
        ProductLongFolderName,
    ];

    public static readonly string[] RootInstallFolderNames =
    [
        ProductShortFolderName,
        ProductFolderName,
        ProductLongFolderName,
    ];

    public static string LocalDataDirectory
    {
        get
        {
            var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            return Path.Combine(local, LocalDataDirectoryName);
        }
    }

    public static string DefaultSavedGamesDirectory
    {
        get
        {
            var documents = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
            return Path.Combine(documents, VendorFolderName, ProductFolderName, SavedGamesDirectoryName);
        }
    }

    public static string CurrentSavePathFileName(int pid)
    {
        return string.Format(CurrentSavePathFileTemplate, pid);
    }
}
