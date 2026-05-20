internal static class Win32ProbeConstants
{
    public const ulong MinUserPointer = 0x10000u;
    public const ulong MaxUserPointer = 0x7FFFFFFFFFFFFFFFul;
    public const int SystemExtendedHandleInformation = 64;
    public const int NtStatusInfoLengthMismatch = unchecked((int)0xC0000004);
    public const int InitialHandleQueryBytes = 0x10000;
    public const int MaxHandleQueryBytes = 0x4000000;
}
