#include "CDCReaderWindows.h"

CDCReaderWindows::CDCReaderWindows(const char *portName)
{
    handle = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (handle == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Failed to open COM port");
    }

    setupPort();
}

CDCReaderWindows::~CDCReaderWindows()
{
    if (handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
    }
}
size_t CDCReaderWindows::readBytes(uint8_t* dst, size_t len)
{
    DWORD read = 0;

    if (!ReadFile(handle, dst, static_cast<DWORD>(len), &read, NULL))
    {
        return 0;
    }

    return static_cast<size_t>(read);
}
size_t CDCReaderWindows::writeBytes(const uint8_t* src, size_t len)
{
    DWORD written = 0;

    if (!WriteFile(handle, src, static_cast<DWORD>(len), &written, NULL))
    {
        return 0;
    }

    return static_cast<size_t>(written);
}

int CDCReaderWindows::overflow(int ch)
{
    if (ch == EOF)
        return 0;
    DWORD written;
    char c = static_cast<char>(ch);
    WriteFile(handle, &c, 1, &written, NULL);
    return ch;
}

int CDCReaderWindows::underflow()
{
    DWORD read;
    char c;
    if (!ReadFile(handle, &c, 1, &read, NULL) || read == 0)
    {
        return EOF;
    }

    buffer = c;
    setg(&buffer, &buffer, &buffer + 1);
    return static_cast<unsigned char>(buffer);
}

void CDCReaderWindows::setupPort()
{
    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);

    GetCommState(handle, &dcb);

    // Baud rate is ignored for CDC, but must be set
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;

    SetCommState(handle, &dcb);

    COMMTIMEOUTS t{};
    t.ReadIntervalTimeout = 1;
    t.ReadTotalTimeoutConstant = 1;
    t.ReadTotalTimeoutMultiplier = 1;
    t.WriteTotalTimeoutConstant = 1;
    t.WriteTotalTimeoutMultiplier = 1;

    SetCommTimeouts(handle, &t);
}
