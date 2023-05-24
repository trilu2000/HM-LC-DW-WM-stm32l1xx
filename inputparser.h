//- -----------------------------------------------------------------------------------------------------------------------
//- serial parser and display functions -----------------------------------------------------------------------------------
//- Parser sketch from: http://jeelabs.org/2010/10/24/parsing-input-commands/
//- -----------------------------------------------------------------------------------------------------------------------


#ifndef __INPUTPARSER_H__
#define __INPUTPARSER_H__


/// Simple parser for input data and one-letter commands
class InputParser {
public:
  typedef struct {
    char code;      // one-letter command code
    byte bytes;     // number of bytes required as input
    void (*fun)();  // code to call for this command
  } Commands;

  /// Set up with a buffer of specified size
  InputParser(byte size, Commands*, Stream & = DSERIAL);
  //InputParser(byte* buf, byte size, Commands*, Stream & = DSERIAL);

  /// Number of data bytes
  byte count() { return fill; }

  /// Call this frequently to check for incoming data
  void poll();

  InputParser& operator >> (char& v) { return get(&v, 1); }
  InputParser& operator >> (byte& v) { return get(&v, 1); }
  InputParser& operator >> (int& v) { return get(&v, 2); }
  InputParser& operator >> (word& v) { return get(&v, 2); }
  InputParser& operator >> (long& v) { return get(&v, 4); }
  InputParser& operator >> (uint32_t& v) { return get(&v, 4); }
  InputParser& operator >> (const char*& v);

  byte* buffer, limit, fill, top, next;
  InputParser& get(void*, byte);

private:
  void reset();

  byte instring, hexmode, hasvalue;
  uint32_t value;
  Commands* cmds;
  Stream& io;
};

extern const InputParser::Commands cmdTab[];


InputParser::InputParser(byte size, Commands* ctab, Stream& stream) : limit(size), cmds(ctab), io(stream) {
  buffer = (byte*)malloc(size);
  reset();
}
void InputParser::reset() {
  fill = next = 0;
  instring = hexmode = hasvalue = 0;
  top = limit;
}

void InputParser::poll() {
  if (!io.available()) return;
  uint8_t ch = io.read();
  //DPRINT(F("hex:"));  DPRINT(hexmode); DPRINT(F(", instr:"));  DPRINT(instring); DPRINT(F(", ch:")); DHEXLN((uint8_t)ch);

  if (ch < ' ' || fill >= top) {
    //DPRINTLN(F("reset"));
    reset();
    return;
  }
  if (instring) {
    if (ch == '"') {
      buffer[fill++] = 0;
      do
        buffer[--top] = buffer[--fill];
      while (fill > value);
      ch = top;
      instring = 0;
    }
    buffer[fill++] = ch;
    return;
  }
  if (hexmode && (('0' <= ch && ch <= '9') || ('A' <= ch && ch <= 'F') || ('a' <= ch && ch <= 'f'))) {
    if (!hasvalue) value = 0;
    if (ch > '9') ch += 9;
    value <<= 4;
    value |= (uint8_t)(ch & 0x0F);
    if (hasvalue) {
      buffer[fill++] = value;
      hasvalue = 0;
    }
    else {
      hasvalue = 1;
    }
    return;
  }
  if ('0' <= ch && ch <= '9') {
    if (!hasvalue) value = 0;
    value = 10 * value + (ch - '0');
    hasvalue = 1;
    return;
  }
  //hexmode = 0;

  switch (ch) {
  case '$':
    hexmode = 1;
    return;
  case '"':
    hexmode = 0;
    instring = 1;
    value = fill;
    return;
  case ':':
    (uint32_t&)buffer[fill] = value;
    fill += 2;
    value >>= 16;
    // fall through
  case '.':
    (uint16_t&)buffer[fill] = value;
    fill += 2;
    hasvalue = 0;
    hexmode = 0;
    return;
  case '-':
    value = -value;
    hasvalue = 0;
    return;
  case ' ':
    if (!hasvalue) return;
    // fall through
  case ',':
    buffer[fill++] = value;
    hasvalue = 0;
    if (ch !=' ') hexmode = 0;
    return;
  }

  if (hasvalue) {
    DPRINT(F("Unrecognized character: ")); DPRINTLN(ch);
    reset();
    return;
  }

  for (Commands* p = cmds; ; ++p) {
    uint8_t code = pgm_read_byte(&p->code);
    if (code == 0)
      break;
    if (ch == code) {
      uint8_t bytes = pgm_read_byte(&p->bytes);
      if (fill < bytes) {
        DPRINT(F("Not enough data, need ")); DPRINT(bytes); DPRINT(F(" bytes"));
      }
      else {
        memset(buffer + fill, 0, top - fill);
        ((void (*)()) pgm_read_word(&p->fun))();
      }
      reset();
      return;
    }
  }

  DPRINT(F("Known commands:"));
  for (Commands* p = cmds; ; ++p) {
    uint8_t code = pgm_read_byte(&p->code);
    if (code == 0)
      break;
    DPRINT(' ');
    DPRINT(code);
  }
  DPRINT('\n');
}

InputParser& InputParser::get(void* ptr, byte len) {
  memcpy(ptr, buffer + next, len);
  next += len;
  return *this;
}

InputParser& InputParser::operator >> (const char*& v) {
  uint8_t offset = buffer[next++];
  v = top <= offset && offset < limit ? (char*)buffer + offset : "";
  return *this;
}

#endif