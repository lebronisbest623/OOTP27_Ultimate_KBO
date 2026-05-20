internal static class PeImageConstants
{
    public const int DosHeaderMinBytes = 0x100;
    public const int PeHeaderPointerOffset = 0x3C;
    public const int CoffHeaderBytes = 0x18;
    public const int BuildIdentityHeaderBytes = 0x58;
    public const int TimestampOffsetFromPeHeader = 8;
    public const int SizeOfImageOffsetFromPeHeader = 0x50;
    public const ushort DosSignature = 0x5A4D;
    public const uint PeSignature = 0x00004550;
    public const uint SectionMemoryExecute = 0x20000000u;
    public const uint SectionMemoryRead = 0x40000000u;
    public const uint SectionMemoryWrite = 0x80000000u;
    public const int SectionHeaderBytes = 40;
}
