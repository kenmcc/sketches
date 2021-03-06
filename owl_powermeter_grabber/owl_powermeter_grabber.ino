// Oregon V2 decoder added - Dominique Pierre
// Oregon V3 decoder revisited - Dominique Pierre
// New code to decode OOK signals from weather sensors, etc.
// 2010-04-11 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php
// $Id: ookDecoder.pde 5331 2010-04-17 10:45:17Z jcw $

class DecodeOOK {
protected:
    byte total_bits, bits, flip, state, pos, data[25];

    virtual char decode (word width) =0;
    
public:

    enum { UNKNOWN, T0, T1, T2, T3, OK, DONE };

    DecodeOOK () { resetDecoder(); }

    bool nextPulse (word width) {
        if (state != DONE)
        
            switch (decode(width)) {
                case -1: resetDecoder(); break;
                case 1:  done(); break;
            }
        return isDone();
    }
    
    bool isDone () const { return state == DONE; }

    const byte* getData (byte& count) const {
        count = pos;
        return data; 
    }
    
    void resetDecoder () {
        total_bits = bits = pos = flip = 0;
        state = UNKNOWN;
    }
    
    // add one bit to the packet data buffer
    
    virtual void gotBit (char value) {
        total_bits++;
        byte *ptr = data + pos;
        *ptr = (*ptr >> 1) | (value << 7);

        if (++bits >= 8) {
            bits = 0;
            if (++pos >= sizeof data) {
                resetDecoder();
                return;
            }
        }
        state = OK;
    }
    
    // store a bit using Manchester encoding
    void manchester (char value) {
        flip ^= value; // manchester code, long pulse flips the bit
        gotBit(flip);
    }
    
    // move bits to the front so that all the bits are aligned to the end
    void alignTail (byte max =0) {
        // align bits
        if (bits != 0) {
            data[pos] >>= 8 - bits;
            for (byte i = 0; i < pos; ++i)
                data[i] = (data[i] >> bits) | (data[i+1] << (8 - bits));
            bits = 0;
        }
        // optionally shift bytes down if there are too many of 'em
        if (max > 0 && pos > max) {
            byte n = pos - max;
            pos = max;
            for (byte i = 0; i < pos; ++i)
                data[i] = data[i+n];
        }
    }
    
    void reverseBits () {
        for (byte i = 0; i < pos; ++i) {
            byte b = data[i];
            for (byte j = 0; j < 8; ++j) {
                data[i] = (data[i] << 1) | (b & 1);
                b >>= 1;
            }
        }
    }
    
    void reverseNibbles () {
        for (byte i = 0; i < pos; ++i)
            data[i] = (data[i] << 4) | (data[i] >> 4);
    }
    
    void done () {
        while (bits)
            gotBit(0); // padding
        state = DONE;
    }
};

// 433 MHz decoders


class OregonDecoderV2 : public DecodeOOK {
public:
    OregonDecoderV2() {}
    
    // add one bit to the packet data buffer
    virtual void gotBit (char value) {
        if(!(total_bits & 0x01))
        {
            data[pos] = (data[pos] >> 1) | (value ? 0x80 : 00);
        }
        total_bits++;
        pos = total_bits >> 4;
        if (pos >= sizeof data) {
            resetDecoder();
            return;
        }
        state = OK;
    }
    
    virtual char decode (word width) {
        if (200 <= width && width < 1200) {
            byte w = width >= 700;
            switch (state) {
                case UNKNOWN:
                    if (w != 0) {
                        // Long pulse
                        ++flip;
                    } else if (32 <= flip) {
                        // Short pulse, start bit
                        flip = 0;
                        state = T0;
                    } else {
                      // Reset decoder
                        return -1;
                    }
                    break;
                case OK:
                    if (w == 0) {
                        // Short pulse
                        state = T0;
                    } else {
                        // Long pulse
                        manchester(1);
                    }
                    break;
                case T0:
                    if (w == 0) {
                      // Second short pulse
                        manchester(0);
                    } else {
                        // Reset decoder
                        return -1;
                    }
                    break;
            }
        } else {
            return -1;
        }
        return total_bits == 160 ? 1: 0;
    }
};

class OregonDecoderV3 : public DecodeOOK {
public:
    OregonDecoderV3() {}
    
    // add one bit to the packet data buffer
    virtual void gotBit (char value) {
        data[pos] = (data[pos] >> 1) | (value ? 0x80 : 00);
        total_bits++;
        pos = total_bits >> 3;
        if (pos >= sizeof data) {
            resetDecoder();
            return;
        }
        state = OK;
    }
    
    virtual char decode (word width) {
        if (200 <= width && width < 1200) {
            byte w = width >= 700;
            switch (state) {
                case UNKNOWN:
                    if (w == 0)
                        ++flip;
                    else if (32 <= flip) {
                        flip = 1;
                        manchester(1);
                    } else
                        return -1;
                    break;
                case OK:
                    if (w == 0)
                        state = T0;
                    else
                        manchester(1);
                    break;
                case T0:
                    if (w == 0)
                        manchester(0);
                    else
                        return -1;
                    break;
            }
        } else {
            return -1;
        }
        return  total_bits == 80 ? 1: 0;
    }
};

class CrestaDecoder : public DecodeOOK {
public:
    CrestaDecoder () {}
    
    virtual char decode (word width) {
        if (200 <= width && width < 1300) {
            byte w = width >= 750;
            switch (state) {
                case UNKNOWN:
                    if (w == 1)
                        ++flip;
                    else if (2 <= flip && flip <= 10)
                        state = T0;
                    else
                        return -1;
                    break;
                case OK:
                    if (w == 0)
                        state = T0;
                    else
                        gotBit(1);
                    break;
                case T0:
                    if (w == 0)
                        gotBit(0);
                    else
                        return -1;
                    break;
            }
        } else if (width >= 2500 && pos >= 7) 
            return 1;
        else
            return -1;
        return 0;
    }
};

class KakuDecoder : public DecodeOOK {
public:
    KakuDecoder () {}
    
    virtual char decode (word width) {
        if (180 <= width && width < 450 || 950 <= width && width < 1250) {
            byte w = width >= 700;
            switch (state) {
                case UNKNOWN:
                case OK:
                    if (w == 0)
                        state = T0;
                    else
                        return -1;
                    break;
                case T0:
                    if (w)
                        state = T1;
                    else
                        return -1;
                    break;
                case T1:
                    state += w + 1;
                    break;
                case T2:
                    if (w)
                        gotBit(0);
                    else
                        return -1;
                    break;
                case T3:
                    if (w == 0)
                        gotBit(1);
                    else
                        return -1;
                    break;
            }
        } else if (width >= 2500 && 8 * pos + bits == 12) {
            for (byte i = 0; i < 4; ++i)
                gotBit(0);
            alignTail(2);
            return 1;
        } else
            return -1;
        return 0;
    }
};

class XrfDecoder : public DecodeOOK {
public:
    XrfDecoder () {}
    
    // see also http://davehouston.net/rf.htm
    virtual char decode (word width) {
        if (width > 2000 && pos >= 4)
            return 1;
        if (width > 5000)
            return -1;
        if (width > 4000 && state == UNKNOWN)
            state = OK;
        else if (350 <= width && width < 1800) {
            byte w = width >= 720;
            switch (state) {
                case OK:
                    if (w == 0)
                        state = T0;
                    else
                        return -1;
                    break;
                case T0:
                    gotBit(w);
                    break;
            }
        } else
            return -1;
        return 0;
    }
};

class HezDecoder : public DecodeOOK {
public:
    HezDecoder () {}
    
    // see also http://homeeasyhacking.wikia.com/wiki/Home_Easy_Hacking_Wiki
    virtual char decode (word width) {
        if (200 <= width && width < 1200) {
            byte w = width >= 600;
            gotBit(w);
        } else if (width >= 5000 && pos >= 5 /*&& 8 * pos + bits == 50*/) {
            for (byte i = 0; i < 6; ++i)
                gotBit(0);
            alignTail(7); // keep last 56 bits
            return 1;
        } else
            return -1;
        return 0;
    }
};

// 868 MHz decoders

class VisonicDecoder : public DecodeOOK {
public:
    VisonicDecoder () {}
    
    virtual char decode (word width) {
        if (200 <= width && width < 1000) {
            byte w = width >= 600;
            switch (state) {
                case UNKNOWN:
                case OK:
                    state = w == 0 ? T0 : T1;
                    break;
                case T0:
                    gotBit(!w);
                    if (w)
                        return 0;
                    break;
                case T1:
                    gotBit(!w);
                    if (!w)
                        return 0;
                    break;
            }
            // sync error, flip all the preceding bits to resync
            for (byte i = 0; i <= pos; ++i)
                data[i] ^= 0xFF; 
        } else if (width >= 2500 && 8 * pos + bits >= 36 && state == OK) {
            for (byte i = 0; i < 4; ++i)
                gotBit(0);
            alignTail(5); // keep last 40 bits
            // only report valid packets
            byte b = data[0] ^ data[1] ^ data[2] ^ data[3] ^ data[4];
            if ((b & 0xF) == (b >> 4))
                return 1;
        } else
            return -1;
        return 0;
    }
};

class EMxDecoder : public DecodeOOK {
public:
    EMxDecoder () {}
    
    // see also http://fhz4linux.info/tiki-index.php?page=EM+Protocol
    virtual char decode (word width) {
        if (200 <= width && width < 1000) {
            byte w = width >= 600;
            switch (state) {
                case UNKNOWN:
                    if (w == 0)
                        ++flip;
                    else if (flip > 20)
                        state = OK;
                    else
                        return -1;
                    break;
                case OK:
                    if (w == 0)
                        state = T0;
                    else
                        return -1;
                    break;
                case T0:
                    gotBit(w);
                    break;
            }
        } else if (width >= 1500 && pos >= 9)
            return 1;
        else
            return -1;
        return 0;
    }
};

class KSxDecoder : public DecodeOOK {
public:
    KSxDecoder () {}
    
    // see also http://www.dc3yc.homepage.t-online.de/protocol.htm
    virtual char decode (word width) {
        if (200 <= width && width < 1000) {
            byte w = width >= 600;
            switch (state) {
                case UNKNOWN:
                    gotBit(w);
                    bits = pos = 0;
                    if (data[0] != 0x95)
                        state = UNKNOWN;
                    break;
                case OK:
                    state = w == 0 ? T0 : T1;
                    break;
                case T0:
                    gotBit(1);
                    if (!w)
                        return -1;
                    break;
                case T1:
                    gotBit(0);
                    if (w)
                        return -1;
                    break;
            }
        } else if (width >= 1500 && pos >= 6) 
            return 1;
        else
            return -1;
        return 0;
    }
};

class FSxDecoder : public DecodeOOK {
public:
    FSxDecoder () {}
    
    // see also http://fhz4linux.info/tiki-index.php?page=FS20%20Protocol
    virtual char decode (word width) {
        if (300 <= width && width < 775) {
            byte w = width >= 500;
            switch (state) {
                case UNKNOWN:
                    if (w == 0)
                        ++flip;
                    else if (flip > 20)
                        state = T1;
                    else
                        return -1;
                    break;
                case OK:
                    state = w == 0 ? T0 : T1;
                    break;
                case T0:
                    gotBit(0);
                    if (w)
                        return -1;
                    break;
                case T1:
                    gotBit(1);
                    if (!w)
                        return -1;
                    break;
            }
        } else if (width >= 1500 && pos >= 5)
            return 1;
        else
            return -1;
        return 0;
    }
};

OregonDecoderV2 orscV2;
OregonDecoderV3 orscV3;
CrestaDecoder cres;
KakuDecoder kaku;
XrfDecoder xrf;
HezDecoder hez;
VisonicDecoder viso;
EMxDecoder emx;
KSxDecoder ksx;
FSxDecoder fsx;

#define PORT 10

volatile word pulse;

void rupt (void) {

    static word last;
    // determine the pulse length in microseconds, for either polarity
    pulse = micros() - last;
    last += pulse;
}

void reportSerial (const char* s, class DecodeOOK& decoder) {
    byte pos;
    unsigned int watts;
    const byte* data = decoder.getData(pos);
    Serial.print(s);
    Serial.print(' ');
    for (byte i = 0; i < pos; ++i) {
        Serial.print(data[i] >> 4, HEX);
        Serial.print(data[i] & 0x0F, HEX);
    }
    
    if (pos > 5) {
      Serial.print('\t');
      watts = ((data[4] * 256) + (data[3] & 0xF0)) * 1.006196884; 
      Serial.print(watts);
    }
    // Serial.print(' ');
    // Serial.print(millis() / 1000);
    
    Serial.println();
    
    decoder.resetDecoder();
}


void setup () {
    Serial.begin(115200);
    Serial.println("\n[ookDecoder]");
    
    //pinMode(PORT, INPUT);  // use
  //attachInterrupt (0, rupt, CHANGE);


}

void loop () {
    static int i = 0;
    cli();
    word p = pulse;
    
    pulse = 0;
    sei();
Serial.println("...");
    
    //if (p != 0){ Serial.print(++i); Serial.print('\n');}
    
    if (p != 0) {
        if (orscV2.nextPulse(p))
            reportSerial("OSV2", orscV2);  
        if (orscV3.nextPulse(p))
            reportSerial("OSV3", orscV3);        
        if (cres.nextPulse(p))
            reportSerial("CRES", cres);        
        if (kaku.nextPulse(p))
            reportSerial("KAKU", kaku);        
        if (xrf.nextPulse(p))
            reportSerial("XRF", xrf);        
        if (hez.nextPulse(p))
            reportSerial("HEZ", hez);        
    }

    if (p != 0) {
        if (viso.nextPulse(p))
            reportSerial("VISO", viso);        
        if (emx.nextPulse(p))
            reportSerial("EMX", emx);        
        if (ksx.nextPulse(p))
            reportSerial("KSX", ksx);        
        if (fsx.nextPulse(p))
            reportSerial("FSX", fsx);        
    }
    */
}
