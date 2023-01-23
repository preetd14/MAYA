# MAYA: An Efficient Hardware-based Framework for Cyber-Deception

## About
This is the source code for MAYA as implemented in the gem5 simulator (version 22.0.0.2) (paper currently submitted to ISCA'23).

Some experiments to test MAYA are included in the Experiments_ISCA23 folder

Currently it contains a WannaCrypt0r ransomware and a SideChannel code for testing

Build instructions on gem5 can be found at their [website](https://www.gem5.org/documentation/general_docs/building) (details in the gem5 folder README)

The changes I made are not directly visible as commits but are as follows (you can easily search them using `DECEPTION_V3`):
- `src/cpu/thread_context.hh`: Deception Table, Honey Quivers, and Exception Synthesis Module are implemented here along with the comments explaining them in detail
- `src/sim/process.hh` : Just the `#define DECEPTION_V3`
- `src/sim/process.cc` : Code to enable deception for a thread and perform initial setup for exception synthesis 
- `src/sim/syscall_emul.hh` : Contains the `openat`, `read`, `stat`, `sendto`, and `clock_gettime` syscalls-relevant deception trigger code
- `src/sim/syscall_emul.cc` : Contains the `connect` syscall-relevant deception trigger code and the MAYA exception subroutines

## Testing Malware
Here I will show you how to test the two malware samples provided in this repository namely, ransomware and side channel.

### WannaCrypt0r
**Without MAYA:**
1. Go into the folder from the base of this repo: `cd Experiments_ISCA23/WannaCrypt0r`
2. Compile the code: `gcc WannaCrypt0r.c -o WannaCrypt0r -lcrypto`
3. Test it on the host to see the original impact on sensitive files: `./WannaCrypt0r ../Experiments_ISCA23/ImpFiles` (I would recomment copying the folder outside so you can recover it once encrypted)
4. If you open the files in there you will see that they now show encrypted contents

**With MAYA:**
1. Go to gem5 from the base of this repo: `cd gem5`
2. If you have not already compiled, do it with the X86 version which will create the compiled objects in the `gem5/build/X86` folder
3. Run this command: `build/X86/gem5.opt configs/example/se.py --cpu-type=DerivO3CPU --cpu-clock=2GHz --caches --l2cache --l1d_size=64kB --l1i_size=32kB --l2_size=2MB --mem-type=DDR4_2400_8x8 --mem-size=8GB --cmd=../Experiments_ISCA23/WannaCrypt0r/WannaCrypt0r --options="../Experiments_ISCA23/ImpFiles"`
4. You will see that all target "sensitive" files inside `/Experiments_ISCA23/ImpFiles` (as listed in `deception_table.txt`) are protected while the files in  `Experiments_ISCA23/DummyFiles` are encrypted

### Side Channel
**Without MAYA:**
1. Go into the folder from the base of this repo: `cd Experiments_ISCA23/SideChannel`
2. Compile the code: `gcc SideChannel.c -o SideChannel -lm`
3. Test it on the host to see the original impact: `./SideChannel 11 13 100` (where the first two numbers are any prime numbers and the 3rd is the message to encrypt which should be less than the product of the two primes)
4. The output will show different timer values for the deceryption process on bits '1' and '0' revealing the key information

**With MAYA:**
1. Go to gem5 from the base of this repo: `cd gem5`
2. If you have not already compiled, do it with the X86 version which will create the compiled objects in the `gem5/build/X86` folder
3. Run this command: `build/X86/gem5.opt configs/example/se.py --cpu-type=DerivO3CPU --cpu-clock=2GHz --caches --l2cache --l1d_size=64kB --l1i_size=32kB --l2_size=2MB --mem-type=DDR4_2400_8x8 --mem-size=8GB --cmd=../Experiments_ISCA23/SideChannel/SideChannel --options="11 13 100"`
4. You can see the timing difference is now even due to the deception clock obfuscating the real clock time by adding the fixed offset given in `deception_table.txt`
