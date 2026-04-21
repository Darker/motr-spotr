#pragma once
#include <streambuf>
#include <windows.h>

class SerialStreamBuf : public std::streambuf
{
  public:
    SerialStreamBuf(const char *portName);

    ~SerialStreamBuf() override;

    size_t readBytes(char *dst, size_t len);

    size_t writeBytes(const char *src, size_t len);

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
