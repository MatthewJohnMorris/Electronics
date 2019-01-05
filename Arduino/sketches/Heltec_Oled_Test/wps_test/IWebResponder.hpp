#ifndef IN_IWEBRESPONDER_HPP
#define IN_IWEBRESPONDER_HPP

class IWebResponder
{
  public:
    virtual String Respond(const String& request) = 0;
    virtual ~IWebResponder() {}
};

#endif // IN_IWEBRESPONDER_HPP
