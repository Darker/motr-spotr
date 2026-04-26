#pragma once
#include <streambuf>
#include <windows.h>

class CDCReaderWindows : public std::streambuf
{
  public:
    CDCReaderWindows(const char *portName);

    ~CDCReaderWindows() override;

    size_t readBytes(uint8_t* dst, size_t len);

    size_t writeBytes(const uint8_t* rc, size_t len);

  protected:
    // Write one character
    int overflow(int ch) override;

    // Read one character
    int underflow() override;

  private:
    void *handle;
    char buffer;

    void setupPort();
};
