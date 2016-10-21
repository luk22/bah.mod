#include "sidengine.h"

extern "C" {


static inline int pfloat_ConvertFromInt(int i)
{
    return i<<16;
}
static inline int pfloat_ConvertFromFloat(float f)
{
    return (int)(f*(1<<16));
}
static inline int pfloat_Multiply(int a, int b)
{
    return (a>>8)*(b>>8);
}
static inline int pfloat_ConvertToInt(int i)
{
    return i>>16;
}

// --------------------------------------------------------- some aux stuff

static inline byte get_bit(dword val, byte b)
{
  return (byte) ((val >> b) & 1);
}

static int opcodes[256]= {
  brk,ora,xxx,xxx,xxx,ora,asl,xxx,php,ora,asl,xxx,xxx,ora,asl,xxx,
  bpl,ora,xxx,xxx,xxx,ora,asl,xxx,clc,ora,xxx,xxx,xxx,ora,asl,xxx,
  jsr,_and,xxx,xxx,bit,_and,rol,xxx,plp,_and,rol,xxx,bit,_and,rol,xxx,
  bmi,_and,xxx,xxx,xxx,_and,rol,xxx,sec,_and,xxx,xxx,xxx,_and,rol,xxx,
  rti,eor,xxx,xxx,xxx,eor,lsr,xxx,pha,eor,lsr,xxx,jmp,eor,lsr,xxx,
  bvc,eor,xxx,xxx,xxx,eor,lsr,xxx,cli,eor,xxx,xxx,xxx,eor,lsr,xxx,
  rts,adc,xxx,xxx,xxx,adc,ror,xxx,pla,adc,ror,xxx,jmp,adc,ror,xxx,
  bvs,adc,xxx,xxx,xxx,adc,ror,xxx,sei,adc,xxx,xxx,xxx,adc,ror,xxx,
  xxx,sta,xxx,xxx,sty,sta,stx,xxx,dey,xxx,txa,xxx,sty,sta,stx,xxx,
  bcc,sta,xxx,xxx,sty,sta,stx,xxx,tya,sta,txs,xxx,xxx,sta,xxx,xxx,
  ldy,lda,ldx,xxx,ldy,lda,ldx,xxx,tay,lda,tax,xxx,ldy,lda,ldx,xxx,
  bcs,lda,xxx,xxx,ldy,lda,ldx,xxx,clv,lda,tsx,xxx,ldy,lda,ldx,xxx,
  cpy,cmp,xxx,xxx,cpy,cmp,dec,xxx,iny,cmp,dex,xxx,cpy,cmp,dec,xxx,
  bne,cmp,xxx,xxx,xxx,cmp,dec,xxx,cld,cmp,xxx,xxx,xxx,cmp,dec,xxx,
  cpx,sbc,xxx,xxx,cpx,sbc,inc,xxx,inx,sbc,nop,xxx,cpx,sbc,inc,xxx,
  beq,sbc,xxx,xxx,xxx,sbc,inc,xxx,sed,sbc,xxx,xxx,xxx,sbc,inc,xxx
};

static int modes[256]= {
 imp,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,xxx,zpx,zpx,xxx,imp,absy,xxx,xxx,xxx,absx,absx,xxx,
 abs,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,xxx,zpx,zpx,xxx,imp,absy,xxx,xxx,xxx,absx,absx,xxx,
 imp,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,xxx,zpx,zpx,xxx,imp,absy,xxx,xxx,xxx,absx,absx,xxx,
 imp,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,ind,abs,abs,xxx,
 rel,indy,xxx,xxx,xxx,zpx,zpx,xxx,imp,absy,xxx,xxx,xxx,absx,absx,xxx,
 imm,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,zpx,zpx,zpy,xxx,imp,absy,acc,xxx,xxx,absx,absx,xxx,
 imm,indx,imm,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,zpx,zpx,zpy,xxx,imp,absy,acc,xxx,absx,absx,absy,xxx,
 imm,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,zpx,zpx,zpx,xxx,imp,absy,acc,xxx,xxx,absx,absx,xxx,
 imm,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,zpx,zpx,zpx,xxx,imp,absy,acc,xxx,xxx,absx,absx,xxx
};


inline int SIDEngine::GenerateDigi(int sIn)
{
    static int last_sample = 0;
    static int sample = 0;

    if (!sample_active) return(sIn);

    if ((sample_position < sample_end) && (sample_position >= sample_start))
    {							
		//Interpolation routine
		//float a = (float)fracPos/(float)mixing_frequency;
		//float b = 1-a;
		//sIn += a*sample + b*last_sample;

        sIn += sample;
		
        fracPos += 985248/sample_period;
		
        if (fracPos > mixing_frequency) 
        {
            fracPos%=mixing_frequency;

            last_sample = sample;			
						
			// Nahstes Samples holen
            if (sample_order == 0) {
                sample_nibble++;						// Nahstes Sample-Nibble
                if (sample_nibble==2) {
                    sample_nibble = 0;
                    sample_position++;
                }
            }
            else {
                sample_nibble--;
                if (sample_nibble < 0) {
                    sample_nibble=1;
                    sample_position++;
                }
            }		
            if (sample_repeats)
            {
                if  (sample_position > sample_end)
                {
                    sample_repeats--;
                    sample_position = sample_repeat_start;
                }						
                else sample_active = 0;
            }
			
            sample = memory[sample_position&0xffff];
            if (sample_nibble==1)	// Hi-Nibble holen?		
                sample = (sample & 0xf0)>>4;
            else sample = sample & 0x0f;
						
			
            sample -= 7;
            sample <<= 10;	
        }
    }

    /* Clipping */    
    /*
    if (sIn < -32767) return -32767;
    else if (sIn > 32767) return 32767;
    */
    

    return (sIn);
}

// initialize SID and frequency dependant values

void SIDEngine::synth_init   (dword mixfrq)
{
  int i;
  mixing_frequency = mixfrq;
  freqmul = 7936000 / mixfrq;
  filtmul = pfloat_ConvertFromFloat(21.5332031f)/mixfrq;
  for (i=0;i<16;i++) {
    attacks [i]=(int) (0x1000000 / (attackTimes[i]*mixfrq));
    releases[i]=(int) (0x1000000 / (decayReleaseTimes[i]*mixfrq));
  }
  memset(&sid,0,sizeof(sid));
  memset(osc,0,sizeof(osc));
  memset(&filter,0,sizeof(filter));
  osc[0].noiseval = 0xffffff;
  osc[1].noiseval = 0xffffff;
  osc[2].noiseval = 0xffffff;  
}

// render a buffer of n samples with the actual register contents
void SIDEngine::synth_render (word *buffer, dword len)
{
  byte v, refosc, outv;
  dword bp;
  byte triout, sawout, plsout, nseout;
  signed short final_sample;
  // step 1: convert the not easily processable sid registers into some
  //         more convenient and fast values (makes the thing much faster
  //         if you process more than 1 sample value at once)
  for (v=0;v<3;v++) {
    osc[v].pulse   = (sid.v[v].pulse & 0xfff) << 16;
    osc[v].filter  = get_bit(sid.res_ftv,v);
    osc[v].attack  = attacks[sid.v[v].ad >> 4];
    osc[v].decay   = releases[sid.v[v].ad & 0xf];
    osc[v].sustain = sid.v[v].sr & 0xf0;
    osc[v].release = releases[sid.v[v].sr & 0xf];
    osc[v].wave    = sid.v[v].wave;
    osc[v].freq    = ((dword)sid.v[v].freq)*freqmul;
  }

#ifdef USE_FILTER
  filter.freq  = (8 * sid.ffreqhi + (sid.ffreqlo&0x7)) * filtmul;
  
 if (filter.freq>pfloat_ConvertFromInt(1))
     filter.freq=pfloat_ConvertFromInt(1);
  // the above line isnt correct at all - the problem is that the filter
  // works only up to rmxfreq/4 - this is sufficient for 44KHz but isnt
  // for 32KHz and lower - well, but sound quality is bad enough then to
  // neglect the fact that the filter doesnt come that high ;)
  filter.l_ena = get_bit(sid.ftp_vol,4);
  filter.b_ena = get_bit(sid.ftp_vol,5);
  filter.h_ena = get_bit(sid.ftp_vol,6);
  filter.v3ena = !get_bit(sid.ftp_vol,7);
  filter.vol   = (sid.ftp_vol & 0xf);
  //filter.rez   = 1.0-0.04*(float)(sid.res_ftv >> 4);
  filter.rez   = pfloat_ConvertFromFloat(1.2f) - 
          pfloat_ConvertFromFloat(0.04f)*(sid.res_ftv >> 4);
  /* We precalculate part of the quick float operation, saves time in loop later */
  filter.rez>>=8;
#endif

  // now render the buffer
  for (bp=0;bp<len;bp++) {
    int outo=0;
    int outf=0;
    // step 2 : generate the two output signals (for filtered and non-
    //          filtered) from the osc/eg sections
    for (v=0;v<3;v++) {
      // update wave counter
      osc[v].counter = (osc[v].counter+osc[v].freq) & 0xFFFFFFF;
      // reset counter / noise generator if reset get_bit set
      if (osc[v].wave & 0x08) {
        osc[v].counter  = 0;
        osc[v].noisepos = 0;
        osc[v].noiseval = 0xffffff;
      }
      refosc = v?v-1:2;  // reference oscillator for sync/ring
      // sync oscillator to refosc if sync bit set
      if (osc[v].wave & 0x02)
        if (osc[refosc].counter < osc[refosc].freq)
          osc[v].counter = osc[refosc].counter * osc[v].freq / osc[refosc].freq;
      // generate waveforms with really simple algorithms
      triout = (byte) (osc[v].counter>>19);
      if (osc[v].counter>>27)
        triout^=0xff;
      sawout = (byte) (osc[v].counter >> 20);
      plsout = (byte) ((osc[v].counter > osc[v].pulse)-1);

      // generate noise waveform exactly as the SID does.
      if (osc[v].noisepos!=(osc[v].counter>>23))
      {
        osc[v].noisepos = osc[v].counter >> 23;
        osc[v].noiseval = (osc[v].noiseval << 1) |
                          (get_bit(osc[v].noiseval,22) ^ get_bit(osc[v].noiseval,17));
        osc[v].noiseout = (get_bit(osc[v].noiseval,22) << 7) |
                          (get_bit(osc[v].noiseval,20) << 6) |
                          (get_bit(osc[v].noiseval,16) << 5) |
                          (get_bit(osc[v].noiseval,13) << 4) |
                          (get_bit(osc[v].noiseval,11) << 3) |
                          (get_bit(osc[v].noiseval, 7) << 2) |
                          (get_bit(osc[v].noiseval, 4) << 1) |
                          (get_bit(osc[v].noiseval, 2) << 0);
      }
      nseout = osc[v].noiseout;

      // modulate triangle wave if ringmod bit set

      if (osc[v].wave & 0x04)
        if (osc[refosc].counter < 0x8000000)
          triout ^= 0xff;

      // now mix the oscillators with an AND operation as stated in
      // the SID's reference manual - even if this is completely wrong.
      // well, at least, the $30 and $70 waveform sounds correct and there's
      // no real solution to do $50 and $60, so who cares.

      outv=0xFF;
      if (osc[v].wave & 0x10) outv &= triout;
      if (osc[v].wave & 0x20) outv &= sawout;
      if (osc[v].wave & 0x40) outv &= plsout;
      if (osc[v].wave & 0x80) outv &= nseout;

      // now process the envelopes. the first thing about this is testing
      // the gate bit and put the EG into attack or release phase if desired
      if (!(osc[v].wave & 0x01)) osc[v].envphase=3;
      else if (osc[v].envphase==3) osc[v].envphase=0;
      // so now process the volume according to the phase and adsr values	  
      switch (osc[v].envphase) {
        case 0 : {                          // Phase 0 : Attack
                   osc[v].envval+=osc[v].attack;
                   if (osc[v].envval >= 0xFFFFFF)
                   {
                     osc[v].envval   = 0xFFFFFF;
                     osc[v].envphase = 1;
                   }
                   break;
                 }
        case 1 : {                          // Phase 1 : Decay
                   osc[v].envval-=osc[v].decay;
                   if ((signed int) osc[v].envval <= (signed int) (osc[v].sustain<<16))
                   {
                     osc[v].envval   = osc[v].sustain<<16;
                     osc[v].envphase = 2;
                   }
                   break;
                 }
        case 2 : {                          // Phase 2 : Sustain
                   if ((signed int) osc[v].envval != (signed int) (osc[v].sustain<<16))
                   {
                     osc[v].envphase = 1;
                   }
                     // :) yes, thats exactly how the SID works. and maybe
                     // a music routine out there supports this, so better
                     // let it in, thanks :)
                   break;
                 }
        case 3 : {                          // Phase 3 : Release
                   osc[v].envval-=osc[v].release;
                   if (osc[v].envval < 0x40000) osc[v].envval= 0x40000;
                     // the volume offset is because the SID does not
                     // completely silence the voices when it should. most
                     // emulators do so though and thats the main reason
                     // why the sound of emulators is too, err... emulated :)
                   break;
                 }
      }
      // now route the voice output to either the non-filtered or the
      // filtered channel and dont forget to blank out osc3 if desired	  
#ifdef USE_FILTER
      if ((v<2) || filter.v3ena)
	  {
        if (osc[v].filter)
          //outf+=((float)osc[v].envval*(float)outv-0x8000000)/0x30000000;
		  outf+=(((int)(outv-0x80))*osc[v].envval)>>22;
		
        else
          //outo+=((float)osc[v].envval*(float)outv-0x8000000)/0x30000000;
                  outo+=(((int)(outv-0x80))*osc[v].envval)>>22;
	  }
#endif
#ifndef USE_FILTER
        
    outf+=((signed short)(outv-0x80)) * (osc[v].envval>>8);
#endif
    }		
    // step 3
    // so, now theres finally time to apply the multi-mode resonant filter
    // to the signal. The easiest thing ist just modelling a real electronic
    // filter circuit instead of fiddling around with complex IIRs or even
    // FIRs ...
    // it sounds as good as them or maybe better and needs only 3 MULs and
    // 4 ADDs for EVERYTHING. SIDPlay uses this kind of filter, too, but
    // Mage messed the whole thing completely up - as the rest of the
    // emulator.
    // This filter sounds a lot like the 8580, as the low-quality, dirty
    // sound of the 6581 is uuh too hard to achieve :)
    
#ifdef USE_FILTER
	
    //filter.h = outf - filter.b*filter.rez - filter.l;	  
	//filter.h = pfloat_ConvertFromInt(outf) - pfloat_Multiply(filter.b, filter.rez) - filter.l;
	filter.h = pfloat_ConvertFromInt(outf) - (filter.b>>8)*filter.rez - filter.l;
	//filter.b += filter.freq * filter.h;
	filter.b += pfloat_Multiply(filter.freq, filter.h);
	//filter.l += filter.freq * filter.b;
        filter.l += pfloat_Multiply(filter.freq, filter.b);

    outf = 0;
	
    if (filter.l_ena) outf+=pfloat_ConvertToInt(filter.l);
    if (filter.b_ena) outf+=pfloat_ConvertToInt(filter.b);
    if (filter.h_ena) outf+=pfloat_ConvertToInt(filter.h);

    final_sample = (signed short) (filter.vol*(outo+outf));
#endif
#ifndef USE_FILTER
    
    final_sample = outf>>10;
#endif
    
    *(buffer+bp)=(signed short) GenerateDigi(final_sample);
  }
}

void SIDEngine::sidPoke(int reg, unsigned char val)
{  	
      int voice=0;

      if ((reg >= 0) && (reg <= 6)) voice=0;
      if ((reg >= 7) && (reg <=13)) {voice=1; reg-=7;}
      if ((reg >= 14) && (reg <=20)) {voice=2; reg-=14;}
  
      switch (reg) {
        case 0: { // Frequenz niederwertiges byte Stimme 1
				  sid.v[voice].freq = (sid.v[voice].freq&0xff00)+val;
                  break;
                }
        case 1: { // Frequenz hoherwertiges byte Stimme 1
			      sid.v[voice].freq = (sid.v[voice].freq&0xff)+(val<<8);
			      break;
		}
		case 2: { // Pulsbreite niederwertiges byte Stimme 1
				  sid.v[voice].pulse = (sid.v[voice].pulse&0xff00)+val;
				  break;
				}
		case 3: { // Pulsbreite hoherwertiges byte Stimme 1
				  sid.v[voice].pulse = (sid.v[voice].pulse&0xff)+(val<<8);
				  break;
				}
		case 4: { sid.v[voice].wave = val; 
            /* Directly look at GATE-Bit!
             * a change may happen twice or more often during one cpujsr
             * Put the Envelope Generator into attack or release phase if desired 
            */
            if ((val & 0x01) == 0) osc[voice].envphase=3;
            else if (osc[voice].envphase==3) osc[voice].envphase=0;
            break;
        }

		case 5: { sid.v[voice].ad = val; break;}
		case 6: { sid.v[voice].sr = val; break;}

		case 21: { sid.ffreqlo = val; break; }
		case 22: { sid.ffreqhi = val; break; }
		case 23: { sid.res_ftv = val; break; }
		case 24: { sid.ftp_vol = val; break;}
	}
  return;
}


byte SIDEngine::getmem(word addr)
{
  if (addr == 0xdd0d) memory[addr]=0;
  return memory[addr];
}

void SIDEngine::setmem(word addr, byte value)
{
  
    memory[addr]=value;

  //#ifdef TRACE
  //printf("setmem: $%04x <- $%02x\n",addr,memory[addr]);
  //#endif

  if ((addr&0xfc00)==0xd400)
  {
    //addr&=0x1f;
    sidPoke(addr&0x1f,value);    
    // Neue SID-Register
    if ((addr > 0xd418) && (addr < 0xd500))
    {                
        // Start-Hi
        if (addr == 0xd41f) internal_start = (internal_start&0x00ff) | (value<<8);
	  // Start-Lo
        if (addr == 0xd41e) internal_start = (internal_start&0xff00) | (value);
	  // Repeat-Hi
        if (addr == 0xd47f) internal_repeat_start = (internal_repeat_start&0x00ff) | (value<<8);
	  // Repeat-Lo
        if (addr == 0xd47e) internal_repeat_start = (internal_repeat_start&0xff00) | (value);

	  // End-Hi
        if (addr == 0xd43e) {
            internal_end = (internal_end&0x00ff) | (value<<8);
        }
	  // End-Lo
        if (addr == 0xd43d) {
            internal_end = (internal_end&0xff00) | (value);
        }
	  // Loop-Size 
        if (addr == 0xd43f) internal_repeat_times = value;
	  // Period-Hi
        if (addr == 0xd45e) internal_period = (internal_period&0x00ff) | (value<<8);
	  // Period-Lo
        if (addr == 0xd45d) {
            internal_period = (internal_period&0xff00) | (value);
        }
	  // Sample Order
        if (addr == 0xd47d) internal_order = value;
	  // Sample Add
        if (addr == 0xd45f) internal_add = value;	  
	  // Start-Sampling
        if (addr == 0xd41d)
        {		  		  		  		  		  		 		  
            sample_repeats = internal_repeat_times;
            sample_position = internal_start;
            sample_start = internal_start; 
            sample_end = internal_end;
            sample_repeat_start = internal_repeat_start;
            sample_period = internal_period;
            sample_order = internal_order;
            switch (value)
            {
                case 0xfd: sample_active = 0; break;
                case 0xfe: 
                case 0xff: sample_active = 1; break;
                
                default: return;
            }            

        }
    }
  
  }

}

byte SIDEngine::getaddr(int mode)
{
  word ad,ad2;  
  switch(mode)
  {
    case imp:
      cycles+=2;
      return 0;
    case imm:
      cycles+=2;
      return getmem(pc++);
    case abs:
      cycles+=4;
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      return getmem(ad);
    case absx:
      cycles+=4;
      ad=getmem(pc++);
      ad|=256*getmem(pc++);
      ad2=ad+x;
      if ((ad2&0xff00)!=(ad&0xff00))
        cycles++;
      return getmem(ad2);
    case absy:
      cycles+=4;
      ad=getmem(pc++);
      ad|=256*getmem(pc++);
      ad2=ad+y;
      if ((ad2&0xff00)!=(ad&0xff00))
        cycles++;
      return getmem(ad2);
    case zp:
      cycles+=3;
      ad=getmem(pc++);
      return getmem(ad);
    case zpx:
      cycles+=4;
      ad=getmem(pc++);
      ad+=x;
      return getmem(ad&0xff);
    case zpy:
      cycles+=4;
      ad=getmem(pc++);
      ad+=y;
      return getmem(ad&0xff);
    case indx:
      cycles+=6;
      ad=getmem(pc++);
      ad+=x;
      ad2=getmem(ad&0xff);
      ad++;
      ad2|=getmem(ad&0xff)<<8;
      return getmem(ad2);
    case indy:
      cycles+=5;
      ad=getmem(pc++);
      ad2=getmem(ad);
      ad2|=getmem((ad+1)&0xff)<<8;
      ad=ad2+y;
      if ((ad2&0xff00)!=(ad&0xff00))
        cycles++;
      return getmem(ad);
    case acc:
      cycles+=2;
      return a;
  }  
  return 0;
}


void SIDEngine::setaddr(int mode, byte val)
{
  word ad,ad2;
  switch(mode)
  {
    case abs:
      cycles+=2;
      ad=getmem(pc-2);
      ad|=256*getmem(pc-1);
      setmem(ad,val);
      return;
    case absx:
      cycles+=3;
      ad=getmem(pc-2);
      ad|=256*getmem(pc-1);
      ad2=ad+x;
      if ((ad2&0xff00)!=(ad&0xff00))
        cycles--;
      setmem(ad2,val);
      return;
    case zp:
      cycles+=2;
      ad=getmem(pc-1);
      setmem(ad,val);
      return;
    case zpx:
      cycles+=2;
      ad=getmem(pc-1);
      ad+=x;
      setmem(ad&0xff,val);
      return;
    case acc:
      a=val;
      return;
  }
}


void SIDEngine::putaddr(int mode, byte val)
{
  word ad,ad2;
  switch(mode)
  {
    case abs:
      cycles+=4;
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      setmem(ad,val);
      return;
    case absx:
      cycles+=4;
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      ad2=ad+x;
      setmem(ad2,val);
      return;
    case absy:
      cycles+=4;
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      ad2=ad+y;
      if ((ad2&0xff00)!=(ad&0xff00))
        cycles++;
      setmem(ad2,val);
      return;
    case zp:
      cycles+=3;
      ad=getmem(pc++);
      setmem(ad,val);
      return;
    case zpx:
      cycles+=4;
      ad=getmem(pc++);
      ad+=x;
      setmem(ad&0xff,val);
      return;
    case zpy:
      cycles+=4;
      ad=getmem(pc++);
      ad+=y;
      setmem(ad&0xff,val);
      return;
    case indx:
      cycles+=6;
      ad=getmem(pc++);
      ad+=x;
      ad2=getmem(ad&0xff);
      ad++;
      ad2|=getmem(ad&0xff)<<8;
      setmem(ad2,val);
      return;
    case indy:
      cycles+=5;
      ad=getmem(pc++);
      ad2=getmem(ad);
      ad2|=getmem((ad+1)&0xff)<<8;
      ad=ad2+y;
      setmem(ad,val);
      return;
    case acc:
      cycles+=2;
      a=val;
      return;
  }
}


inline void SIDEngine::setflags(int flag, int cond)
{
  // cond?p|=flag:p&=~flag;
  if (cond) p|=flag;
  else p&=~flag;
}


inline void SIDEngine::push(byte val)
{
  setmem(0x100+s,val);
  if (s) s--;
}

inline byte SIDEngine::pop()
{
  if (s<0xff) s++;
  return getmem(0x100+s);
}

void SIDEngine::branch(int flag)
{
  signed char dist;
  dist=(signed char)getaddr(imm);
  wval=pc+dist;
  if (flag)
  {
    cycles+=((pc&0x100)!=(wval&0x100))?2:1;
    pc=wval;
  }
}

void SIDEngine::cpuReset()
{
  a=x=y=0;
  p=0;
  s=255;
  pc=getaddr(0xfffc);
}

void SIDEngine::cpuResetTo(word npc)
{
  a=0;
  x=0;
  y=0;
  p=0;
  s=255;
  pc=npc;
}

inline void SIDEngine::cpuParse()
{
    unsigned char opc=getmem(pc++);
    int cmd=opcodes[opc];
    int addr=modes[opc];
    int c;  
    switch (cmd)
    {
        case adc:
            wval=(unsigned short)a+getaddr(addr)+((p&FLAG_C)?1:0);
            setflags(FLAG_C, wval&0x100);
            a=(unsigned char)wval;
            setflags(FLAG_Z, !a);
            setflags(FLAG_N, a&0x80);
            setflags(FLAG_V, (!!(p&FLAG_C)) ^ (!!(p&FLAG_N)));
            break;
        case _and:
            bval=getaddr(addr);
            a&=bval;
            setflags(FLAG_Z, !a);
            setflags(FLAG_N, a&0x80);
            break;
        case asl:
            wval=getaddr(addr);
            wval<<=1;
            setaddr(addr,(unsigned char)wval);
            setflags(FLAG_Z,!wval);
            setflags(FLAG_N,wval&0x80);
            setflags(FLAG_C,wval&0x100);
            break;
        case bcc:
            branch(!(p&FLAG_C));
            break;
        case bcs:
            branch(p&FLAG_C);
            break;
        case bne:
            branch(!(p&FLAG_Z));
            break;
        case beq:
            branch(p&FLAG_Z);
            break;
        case bpl:
            branch(!(p&FLAG_N));
            break;
        case bmi:
            branch(p&FLAG_N);
            break;
        case bvc:
            branch(!(p&FLAG_V));
            break;
        case bvs:
            branch(p&FLAG_V);
            break;
        case bit:
            bval=getaddr(addr);
            setflags(FLAG_Z,!(a&bval));
            setflags(FLAG_N,bval&0x80);
            setflags(FLAG_V,bval&0x40);
            break;
        case brk:
            pc=0;           /* Just quit the emulation */
            break;
        case clc:
            setflags(FLAG_C,0);
            break;
        case cld:
            setflags(FLAG_D,0);
            break;
        case cli:
            setflags(FLAG_I,0);
            break;
        case clv:
            setflags(FLAG_V,0);
            break;
        case cmp:
            bval=getaddr(addr);
            wval=(unsigned short)a-bval;
            setflags(FLAG_Z,!wval);
            setflags(FLAG_N,wval&0x80);
            setflags(FLAG_C,a>=bval);
            break;
        case cpx:
            bval=getaddr(addr);
            wval=(unsigned short)x-bval;
            setflags(FLAG_Z,!wval);
            setflags(FLAG_N,wval&0x80);      
            setflags(FLAG_C,x>=bval);
            break;
        case cpy:
            bval=getaddr(addr);
            wval=(unsigned short)y-bval;
            setflags(FLAG_Z,!wval);
            setflags(FLAG_N,wval&0x80);      
            setflags(FLAG_C,y>=bval);
            break;
        case dec:
            bval=getaddr(addr);
            bval--;
            setaddr(addr,bval);
            setflags(FLAG_Z,!bval);
            setflags(FLAG_N,bval&0x80);
            break;
        case dex:
            x--;
            setflags(FLAG_Z,!x);
            setflags(FLAG_N,x&0x80);
            break;
        case dey:
            y--;
            setflags(FLAG_Z,!y);
            setflags(FLAG_N,y&0x80);
            break;
        case eor:
            bval=getaddr(addr);
            a^=bval;
            setflags(FLAG_Z,!a);
            setflags(FLAG_N,a&0x80);
            break;
        case inc:
            bval=getaddr(addr);
            bval++;
            setaddr(addr,bval);
            setflags(FLAG_Z,!bval);
            setflags(FLAG_N,bval&0x80);
            break;
        case inx:
            x++;
            setflags(FLAG_Z,!x);
            setflags(FLAG_N,x&0x80);
            break;
        case iny:
            y++;
            setflags(FLAG_Z,!y);
            setflags(FLAG_N,y&0x80);
            break;
        case jmp:
            wval=getmem(pc++);
            wval|=256*getmem(pc++);
            switch (addr)
            {
                case abs:
                    pc=wval;
                    break;
                case ind:
                    pc=getmem(wval);
                    pc|=256*getmem(wval+1);
                    break;
            }
            break;
        case jsr:
            push((pc+1)>>8);
            push((pc+1));
            wval=getmem(pc++);
            wval|=256*getmem(pc++);
            pc=wval;
            break;
        case lda:
            a=getaddr(addr);
            setflags(FLAG_Z,!a);
            setflags(FLAG_N,a&0x80);
            break;
        case ldx:
            x=getaddr(addr);
            setflags(FLAG_Z,!x);
            setflags(FLAG_N,x&0x80);
            break;
        case ldy:
            y=getaddr(addr);
            setflags(FLAG_Z,!y);
            setflags(FLAG_N,y&0x80);
            break;
        case lsr:      
            bval=getaddr(addr); wval=(unsigned char)bval;
            wval>>=1;
            setaddr(addr,(unsigned char)wval);
            setflags(FLAG_Z,!wval);
            setflags(FLAG_N,wval&0x80);
            setflags(FLAG_C,bval&1);
            break;
        case nop:
            break;
        case ora:
            bval=getaddr(addr);
            a|=bval;
            setflags(FLAG_Z,!a);
            setflags(FLAG_N,a&0x80);
            break;
        case pha:
            push(a);
            break;
        case php:
            push(p);
            break;
        case pla:
            a=pop();
            setflags(FLAG_Z,!a);
            setflags(FLAG_N,a&0x80);
            break;
        case plp:
            p=pop();
            break;
        case rol:
            bval=getaddr(addr);
            c=!!(p&FLAG_C);
            setflags(FLAG_C,bval&0x80);
            bval<<=1;
            bval|=c;
            setaddr(addr,bval);
            setflags(FLAG_N,bval&0x80);
            setflags(FLAG_Z,!bval);
            break;
        case ror:
            bval=getaddr(addr);
            c=!!(p&FLAG_C);
            setflags(FLAG_C,bval&1);
            bval>>=1;
            bval|=128*c;
            setaddr(addr,bval);
            setflags(FLAG_N,bval&0x80);
            setflags(FLAG_Z,!bval);
            break;
        case rti:
            /* Treat RTI like RTS */
        case rts:
            wval=pop();
            wval|=pop()<<8;
            pc=wval+1;
            break;
        case sbc:      
            bval=getaddr(addr)^0xff;
            wval=(unsigned short)a+bval+((p&FLAG_C)?1:0);
            setflags(FLAG_C, wval&0x100);
            a=(unsigned char)wval;
            setflags(FLAG_Z, !a);
            setflags(FLAG_N, a>127);
            setflags(FLAG_V, (!!(p&FLAG_C)) ^ (!!(p&FLAG_N)));
            break;
        case sec:
            setflags(FLAG_C,1);
            break;
        case sed:
            setflags(FLAG_D,1);
            break;
        case sei:
            setflags(FLAG_I,1);
            break;
        case sta:
            putaddr(addr,a);
            break;
        case stx:
            putaddr(addr,x);
            break;
        case sty:
            putaddr(addr,y);
            break;
        case tax:
            x=a;
            setflags(FLAG_Z, !x);
            setflags(FLAG_N, x&0x80);
            break;
        case tay:
            y=a;
            setflags(FLAG_Z, !y);
            setflags(FLAG_N, y&0x80);
            break;
        case tsx:
            x=s;
            setflags(FLAG_Z, !x);
            setflags(FLAG_N, x&0x80);
            break;
        case txa:
            a=x;
            setflags(FLAG_Z, !a);
            setflags(FLAG_N, a&0x80);
            break;
        case txs:
            s=x;
            break;
        case tya:
            a=y;
            setflags(FLAG_Z, !a);
            setflags(FLAG_N, a&0x80);
            break;  
    }        
}

int SIDEngine::cpuJSR(word npc, byte na)
{
  int ccl;
  
  a=na;
  x=0;
  y=0;
  p=0;
  s=255;
  pc=npc;
  push(0);
  push(0);
  ccl=0;    
  while (pc)
  {
    cpuParse();
  }  
  return 0;
}




void SIDEngine::c64Init(void)
{
  int i;
  
  cycles = 0;

  synth_init(44100);

  for(i=0; i<65536; i++)
    memory[i]=0;
  cpuReset();  
  
  sidPoke(24,15);
}

unsigned short SIDEngine::LoadSIDFromMemory(void *pSidData, unsigned short *load_addr,
                                 unsigned short *init_addr, unsigned short *play_addr, unsigned char *subsongs, unsigned char *startsong, unsigned char *speed, unsigned short size)
{
    unsigned char *pData;
    unsigned char data_file_offset;
	enum enFileFormat {FILEFORMAT_NONE, FILEFORMAT_PSID, FILEFORMAT_RSID};
	int nFileFormat = FILEFORMAT_NONE;
	
	pData = (unsigned char*)pSidData;

	if (strncmp((char*)pData, "PSID", 4) == 0) nFileFormat = FILEFORMAT_PSID;
	else if (strncmp((char*)pData, "RSID", 4) == 0) nFileFormat = FILEFORMAT_RSID;

    
    data_file_offset = pData[7];

    *load_addr = pData[8]<<8;
    *load_addr|= pData[9];

    *init_addr = pData[10]<<8;
    *init_addr|= pData[11];

    *play_addr = pData[12]<<8;
    *play_addr|= pData[13];

    *subsongs = pData[0xf]-1;
    *startsong = pData[0x11]-1;

    if (*load_addr == 0) {
		*load_addr = pData[data_file_offset];
		*load_addr|= pData[data_file_offset+1]<<8;
	}
        
    *speed = pData[0x15];
    
    memset(memory, 0, sizeof(memory));
    memcpy(&memory[*load_addr], &pData[data_file_offset+2], size-(data_file_offset+2));
    
    if (*play_addr == 0)
    {
        cpuJSR(*init_addr, 0);
        *play_addr = (memory[0x0315]<<8)+memory[0x0314];

        if (*play_addr == 0) {
            *play_addr = (memory[0xffff]<<8)+memory[0xfffe];
        }
    }

    return *load_addr;
}

word SIDEngine::c64SidLoad(unsigned char *data, int size, word *init_addr, word *play_addr, byte *sub_song_start, byte *max_sub_songs, byte *speed, char *name, char *author, char *copyright)
{

	if (size <= 100) return(0);

	int i;
	unsigned char * d = data;
	int count = 0;
	
	// Check header
	char PSID[4];

	strncpy(PSID, (char*)d, 4);
	if (strncmp(PSID, "PSID", 4) != 0) return 0;
	d+=22;
	
	// Name holen
	strncpy(name, (char*)d, 32);
	d+=32;
	
	// Author holen
	strncpy(author, (char*)d, 32);
	d+=32;
	
	// Copyright holen
	strncpy(copyright, (char*)d, 32);
	
	
    unsigned char sidmem[65536];
    unsigned char *p = sidmem;
    unsigned short load_addr;
	
	for (i=0; i<size; i++) {
		*p++ = data[i];
	}
    
    LoadSIDFromMemory(sidmem, &load_addr,
                      init_addr, play_addr, max_sub_songs, sub_song_start, speed, p-sidmem);
    name = NULL; author = NULL; copyright = NULL;
    
        
    return load_addr;
}


}


