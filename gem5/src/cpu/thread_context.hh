/*
 * Copyright (c) 2011-2012, 2016-2018, 2020 ARM Limited
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2006 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __CPU_THREAD_CONTEXT_HH__
#define __CPU_THREAD_CONTEXT_HH__

#define DECEPTION_V3

#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#include <iostream>
#include <string>

#include "arch/generic/htm.hh"
#include "arch/generic/isa.hh"
#include "arch/generic/pcstate.hh"
#include "arch/vecregs.hh"
#include "base/types.hh"
#include "config/the_isa.hh"
#include "cpu/pc_event.hh"
#include "cpu/reg_class.hh"

#ifdef DECEPTION_V3
#define NUM_QUIVER_ENTRIES 5
#define PATH_SIZE 50
#define GARBARGE_SIZE 1024
#define GARBAGE_MIN 33
#define GARBAGE_MAX 126
#define QUIVER_PORT 30800
#define QUIVER_IP "127.0.0.1"
//#define QUIVER_STAT_MODE 16877
//#define QUIVER_TIMER_OFFSET 60
#endif

using namespace std;

namespace gem5
{

// @todo: Figure out a more architecture independent way to obtain the ITB and
// DTB pointers.
namespace TheISA
{
    class Decoder;
}
class BaseCPU;
class BaseMMU;
class BaseTLB;
class CheckerCPU;
class Checkpoint;
class InstDecoder;
class PortProxy;
class Process;
class System;
class Packet;
using PacketPtr = Packet *;

/**
 * ThreadContext is the external interface to all thread state for
 * anything outside of the CPU. It provides all accessor methods to
 * state that might be needed by external objects, ranging from
 * register values to things such as kernel stats. It is an abstract
 * base class; the CPU can create its own ThreadContext by
 * deriving from it.
 *
 * The ThreadContext is slightly different than the ExecContext.  The
 * ThreadContext provides access to an individual thread's state; an
 * ExecContext provides ISA access to the CPU (meaning it is
 * implicitly multithreaded on SMT systems).  Additionally the
 * ThreadState is an abstract class that exactly defines the
 * interface; the ExecContext is a more implicit interface that must
 * be implemented so that the ISA can access whatever state it needs.
 */
class ThreadContext : public PCEventScope
{
  protected:
    bool useForClone = false;

  public:

#if defined(DECEPTION_V3)
    /*The functions and members in this file work with 5 tested syscalls for MAYA deception (ISCA23). In the current implementation in
    syscall_emul.hh/cc files where we implemented subroutines, the target field depends on each syscall. For example, for openat, the 
    target field is the target sensitive file to be protected since we wanted to avoid deceiving the open syscalls called outside of user code
    (could be kernel or glibc wrapper). 
    ToDo: Need to find a way to differentiate between user code and glibc wrapper code/kernel code so that the target field can have its 
    intended purpose i.e. to deceive a targeted register value (directly or indirectly).*/

    /*The following code is a part of the Exception Synthesis Module which currently creates an internal structure of the static deception 
    table stored as a file and initializes the honey quivers based on pre-set parameters (#defines lines 70-76). Additionally theis module should
    also create snippets of ASM code to execute deception functionality dynamically when triggered by the Deception Trigger but currently
    the Deception Trigger is implemented inside individual syscalls that are a part of our effectiveness evaluation and hence the code snippets
    are also implemented as two methods, one for syscall time deception and the other for sysret time. In a real hardware the trigger logic
    along with the ASM blocks can be created to form a cohesive runtime deception process just like it is done here via code.*/
    
    //Honey quivers to store the path, garbage and struct type quivers
    typedef struct {
        char path_quiver[NUM_QUIVER_ENTRIES][PATH_SIZE];
        char *garbage_quiver = (char*)malloc(GARBARGE_SIZE*sizeof(char));
        struct sockaddr sockaddr_quiver[NUM_QUIVER_ENTRIES];
    } Quivers;

    //MAYA supports three primitives for deception.
    enum Primitives {
        PRIMITIVE_NOT_FOUND = 0,
        REPLACE_ENTER = 1,
        REPLACE_RETURN = 2,
        SCRAMBLE_RETURN = 3
    };

    //Converts strings to enum values
    Primitives convert_primitives(const string& str) {
        if (str == "REPLACE_ENTER") return REPLACE_ENTER;
        else if(str == "REPLACE_RETURN") return REPLACE_RETURN;
        else if(str == "SCRAMBLE_RETURN") return SCRAMBLE_RETURN;
        else return PRIMITIVE_NOT_FOUND;
    }

    //MAYA supports four modes of deception that will determine the appropriate honey resources to be supplied to the targeted syscalls
    enum Modes {
        MODE_NOT_FOUND = 0,
        INDIRECT = 1,
        DIRECT = 2,
        INDIRECT_REGS = 3,
        REGS = 4
    };

    //Converts strings to enum values
    Modes convert_modes(const string& str) {
        if (str == "INDIRECT") return INDIRECT;
        else if(str == "DIRECT") return DIRECT;
        else if(str == "INDIRECT_REGS") return INDIRECT_REGS;
        else if(str == "REGS") return REGS;
        else return MODE_NOT_FOUND;
    }

    //The internal deception entries struct called as Params that hold the various parameters required by the MAYA framework to modify syscalls
    typedef struct {
        unsigned int sysnum = 0;
        unsigned int entries = 0;
        Primitives primitive = PRIMITIVE_NOT_FOUND;
        char *target_field = (char *)malloc(128*sizeof(char));
        Modes mode = MODE_NOT_FOUND;
        struct Data {
            unsigned int honey_value = 0;
            
            struct QuiverAddress {
                unsigned long quiver_base_addr = 0;
                unsigned int quiver_stride = 0;
                unsigned int max_bytes = 0;
            };
            QuiverAddress quiveraddr;
            
            struct ArchRegs {
                unsigned int numregs = 0;
                char reg_name[6][4];
            };
            ArchRegs archregs;
        };
        Data data;
    } Params;

    bool deceptionThread = false; //Additional knob that can be switched on/off via command line in the future (currently always on)
    vector<Params> deceptionTable; //The internal deception table structure which will be used by the synthesis module
    vector<Params> deceptionParams; //A set of deception entries for a speciic syscall
    Quivers honeyQuiver; //An object of the honey Quivers

    //Initialize the deception table as will be stored internally
    void initDeceptionTable (){
        string dLine, dWord;
        fstream dFile;
        deceptionTable.clear(); //need to clear in case of multi-threaded execution
        
        dFile.open("/home/preet_derasari/gem5/deception_table.txt", ios::in); //Modify this according to your path
        if(dFile.is_open()) {
            while(getline(dFile,dLine)) {
                Params p; //A temporary object to store fetched parameters
                
                stringstream str(dLine);
                
                //Get the syscall number
                getline(str, dWord, ',');
                p.sysnum = atoi(dWord.c_str());

                //Get the number of entries for each syscall
                /*ToDo: Technically if the number of entries for a syscall is > 1 then we need to do a loop here where the rest of the 
                parameters are initialized for the same syscall number thus not bloating up the deceptionParams but since the current
                Params struct does not have this ability we just do it at findDeceptionParams method. Basically need to create a vector of 
                everything after entries inside of Params struct (less memory intensive) to avoid creating a vector of deceptionParams itself*/
                getline(str, dWord, ',');
                p.entries = atoi(dWord.c_str());
                
                //Get the deception primitive to be applied for the syscall
                getline(str, dWord, ',');
                p.primitive = convert_primitives(dWord);
                
                //Get the target location where deception is to be applied
                getline(str, dWord, ',');
                strcpy(p.target_field, (char *)(dWord.c_str()));

                //Get the mode of deception
                getline(str, dWord, ',');
                p.mode = convert_modes(dWord.c_str());
                
                //Initialize honey resouce specifier for DIRECT mode of deception with the honey honey_value
                if (p.mode == DIRECT) {
                    getline(str, dWord, ',');
                    p.data.honey_value = atoi(dWord.c_str());
                }

                //Initialize honey resource specifier for REGS and INDIRECT_REGS modes of deception
                if (p.mode == INDIRECT_REGS || p.mode == REGS) {
                    getline(str, dWord, ',');
                    p.data.archregs.numregs = atoi(dWord.c_str());
                    for(int i = 0; i < p.data.archregs.numregs; i++){
                        getline(str, dWord, ',');
                        strcpy(p.data.archregs.reg_name[i], (char *)(dWord.c_str())); 
                    }
                }
                
                //Store the temporary object in the main vector
                deceptionTable.push_back(p);
            }
            dFile.close();
            dLine.clear();
            dWord.clear();

            //Uncomment if you want to debug
            /*cout<<"==GEM5==The deception table looks like this:"<<endl;
            for (int i = 0; i < deceptionTable.size(); i++)
            {
                cout<<deceptionTable[i].sysnum<<"\t"<<deceptionTable[i].entries<<"\t"<<deceptionTable[i].primitive<<"\t"<<deceptionTable[i].target_field<<"\t"<<deceptionTable[i].mode<<endl;
            }*/
        }
        else {
            printf("Could not open table.txt\n");
        }
    }
    
    //This can be done by reading from multiple files in the future where each file has multiple number of honey values for each type of quiver 
    void fillHoneyQuivers() {
        int i;

        //Initialize the object of the Quivers struct
        honeyQuiver = Quivers();

        //Populate path quiver with files
        for(i = 0; i < NUM_QUIVER_ENTRIES; i++){
            sprintf(honeyQuiver.path_quiver[i], "/home/preet_derasari/DummyFiles/fake_test%d.txt", i);
        }
        
        //populate garbage quiver with random bytes
        for(i = 0; i < GARBARGE_SIZE; i++){
            honeyQuiver.garbage_quiver[i] = (char)(rand() % (GARBAGE_MAX - GARBAGE_MIN + 1) + GARBAGE_MIN);
        }
        
        //populate sockaddr quiver with honey servers (different port numbers for now)
        struct sockaddr_in serv_addr;
        for(i = 0; i < NUM_QUIVER_ENTRIES; i++){
            memset(&serv_addr, '0', sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(QUIVER_PORT + i);
            inet_pton(AF_INET, QUIVER_IP, &serv_addr.sin_addr);
            memcpy(&honeyQuiver.sockaddr_quiver[i], (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        }
    }

    /*This is a makeshift as per current implementation where the "exception subroutines" will be supplied with the proper honey quiver address
    values at runtime for INDIRECT and INDIRECT_REGS modes of deception*/
    void initHoneyResourceSpecifier(){
        for (int i = 0; i < deceptionTable.size(); i++)
        {
            if(deceptionTable[i].mode == INDIRECT || deceptionTable[i].mode == INDIRECT_REGS) {
                if(deceptionTable[i].sysnum == 257) {
                    deceptionTable[i].data.quiveraddr.quiver_base_addr = (long)honeyQuiver.path_quiver;
                    deceptionTable[i].data.quiveraddr.quiver_stride = PATH_SIZE; //assumes all sizes are same
                    deceptionTable[i].data.quiveraddr.max_bytes = NUM_QUIVER_ENTRIES * PATH_SIZE; //can be calculated using sizeof
                }
                if(deceptionTable[i].sysnum == 42) {
                    deceptionTable[i].data.quiveraddr.quiver_base_addr = (long)honeyQuiver.sockaddr_quiver;
                    deceptionTable[i].data.quiveraddr.quiver_stride = sizeof(honeyQuiver.sockaddr_quiver[0]); //assumes all sizes are same
                    deceptionTable[i].data.quiveraddr.max_bytes = sizeof(honeyQuiver.sockaddr_quiver);
                }
                if(deceptionTable[i].sysnum == 44) {
                    deceptionTable[i].data.quiveraddr.quiver_base_addr = (long)honeyQuiver.garbage_quiver;
                    deceptionTable[i].data.quiveraddr.quiver_stride = sizeof(honeyQuiver.garbage_quiver[0]); //assumes all sizes are same
                    deceptionTable[i].data.quiveraddr.max_bytes = strlen(honeyQuiver.garbage_quiver);
                }
            }
        }
    }

    //Find all deception table entries that correspond to a given syscall number
    bool findDeceptionParams(int sysnum) {
        bool found = false;
        int numentries = 0;
        deceptionParams.clear();
        for (int i = 0; i < deceptionTable.size(); i++) {
            if(deceptionTable[i].sysnum == sysnum && deceptionTable[i].entries != numentries) {
                deceptionParams.push_back(deceptionTable[i]);
                numentries++;
                found = true;
            }
        }
        return found;
    }

    //Check whether the current thread is enabled for deception
    //ToDo: Check if next two functions can be moved to somewhere where multiple threads of a process can be selectively enabled for deception.
    bool isDeceptionThread() { return deceptionThread; }
    
    //Set deception ON/OFF for the current thread
    void setDeceptionThread(bool d) { deceptionThread = d; }
#endif

    bool getUseForClone() { return useForClone; }

    void setUseForClone(bool new_val) { useForClone = new_val; }

    enum Status
    {
        /// Running.  Instructions should be executed only when
        /// the context is in this state.
        Active,

        /// Temporarily inactive.  Entered while waiting for
        /// synchronization, etc.
        Suspended,

        /// Trying to exit and waiting for an event to completely exit.
        /// Entered when target executes an exit syscall.
        Halting,

        /// Permanently shut down.  Entered when target executes
        /// m5exit pseudo-instruction.  When all contexts enter
        /// this state, the simulation will terminate.
        Halted
    };

    virtual ~ThreadContext() { };

    virtual BaseCPU *getCpuPtr() = 0;

    virtual int cpuId() const = 0;

    virtual uint32_t socketId() const = 0;

    virtual int threadId() const = 0;

    virtual void setThreadId(int id) = 0;

    virtual ContextID contextId() const = 0;

    virtual void setContextId(ContextID id) = 0;

    virtual BaseMMU *getMMUPtr() = 0;

    virtual CheckerCPU *getCheckerCpuPtr() = 0;

    virtual BaseISA *getIsaPtr() const = 0;

    virtual InstDecoder *getDecoderPtr() = 0;

    virtual System *getSystemPtr() = 0;

    virtual void sendFunctional(PacketPtr pkt);

    virtual Process *getProcessPtr() = 0;

    virtual void setProcessPtr(Process *p) = 0;

    virtual Status status() const = 0;

    virtual void setStatus(Status new_status) = 0;

    /// Set the status to Active.
    virtual void activate() = 0;

    /// Set the status to Suspended.
    virtual void suspend() = 0;

    /// Set the status to Halted.
    virtual void halt() = 0;

    /// Quiesce thread context
    void quiesce();

    /// Quiesce, suspend, and schedule activate at resume
    void quiesceTick(Tick resume);

    virtual void takeOverFrom(ThreadContext *old_context) = 0;

    virtual void regStats(const std::string &name) {};

    virtual void scheduleInstCountEvent(Event *event, Tick count) = 0;
    virtual void descheduleInstCountEvent(Event *event) = 0;
    virtual Tick getCurrentInstCount() = 0;

    // Not necessarily the best location for these...
    // Having an extra function just to read these is obnoxious
    virtual Tick readLastActivate() = 0;
    virtual Tick readLastSuspend() = 0;

    virtual void copyArchRegs(ThreadContext *tc) = 0;

    virtual void clearArchRegs() = 0;

    //
    // New accessors for new decoder.
    //
    virtual RegVal getReg(const RegId &reg) const;
    virtual void getReg(const RegId &reg, void *val) const;
    virtual void *getWritableReg(const RegId &reg);

    virtual void setReg(const RegId &reg, RegVal val);
    virtual void setReg(const RegId &reg, const void *val);

    RegVal
    readIntReg(RegIndex reg_idx) const
    {
        return getReg(RegId(IntRegClass, reg_idx));
    }

    RegVal
    readFloatReg(RegIndex reg_idx) const
    {
        return getReg(RegId(FloatRegClass, reg_idx));
    }

    TheISA::VecRegContainer
    readVecReg(const RegId &reg) const
    {
        TheISA::VecRegContainer val;
        getReg(reg, &val);
        return val;
    }
    TheISA::VecRegContainer&
    getWritableVecReg(const RegId& reg)
    {
        return *(TheISA::VecRegContainer *)getWritableReg(reg);
    }

    RegVal
    readVecElem(const RegId& reg) const
    {
        return getReg(reg);
    }

    RegVal
    readCCReg(RegIndex reg_idx) const
    {
        return getReg(RegId(CCRegClass, reg_idx));
    }

    void
    setIntReg(RegIndex reg_idx, RegVal val)
    {
        setReg(RegId(IntRegClass, reg_idx), val);
    }

    void
    setFloatReg(RegIndex reg_idx, RegVal val)
    {
        setReg(RegId(FloatRegClass, reg_idx), val);
    }

    void
    setVecReg(const RegId& reg, const TheISA::VecRegContainer &val)
    {
        setReg(reg, &val);
    }

    void
    setVecElem(const RegId& reg, RegVal val)
    {
        setReg(reg, val);
    }

    void
    setCCReg(RegIndex reg_idx, RegVal val)
    {
        setReg(RegId(CCRegClass, reg_idx), val);
    }

    virtual const PCStateBase &pcState() const = 0;

    virtual void pcState(const PCStateBase &val) = 0;
    void
    pcState(Addr addr)
    {
        std::unique_ptr<PCStateBase> new_pc(getIsaPtr()->newPCState(addr));
        pcState(*new_pc);
    }

    virtual void pcStateNoRecord(const PCStateBase &val) = 0;

    virtual RegVal readMiscRegNoEffect(RegIndex misc_reg) const = 0;

    virtual RegVal readMiscReg(RegIndex misc_reg) = 0;

    virtual void setMiscRegNoEffect(RegIndex misc_reg, RegVal val) = 0;

    virtual void setMiscReg(RegIndex misc_reg, RegVal val) = 0;

    virtual RegId flattenRegId(const RegId& reg_id) const = 0;

    // Also not necessarily the best location for these two.  Hopefully will go
    // away once we decide upon where st cond failures goes.
    virtual unsigned readStCondFailures() const = 0;

    virtual void setStCondFailures(unsigned sc_failures) = 0;

    // This function exits the thread context in the CPU and returns
    // 1 if the CPU has no more active threads (meaning it's OK to exit);
    // Used in syscall-emulation mode when a  thread calls the exit syscall.
    virtual int exit() { return 1; };

    /** function to compare two thread contexts (for debugging) */
    static void compare(ThreadContext *one, ThreadContext *two);

    /** @{ */
    /**
     * Flat register interfaces
     *
     * Some architectures have different registers visible in
     * different modes. Such architectures "flatten" a register (see
     * flattenRegId()) to map it into the
     * gem5 register file. This interface provides a flat interface to
     * the underlying register file, which allows for example
     * serialization code to access all registers.
     */

    virtual RegVal getRegFlat(const RegId &reg) const;
    virtual void getRegFlat(const RegId &reg, void *val) const = 0;
    virtual void *getWritableRegFlat(const RegId &reg) = 0;

    virtual void setRegFlat(const RegId &reg, RegVal val);
    virtual void setRegFlat(const RegId &reg, const void *val) = 0;

    RegVal
    readIntRegFlat(RegIndex idx) const
    {
        return getRegFlat(RegId(IntRegClass, idx));
    }
    void
    setIntRegFlat(RegIndex idx, RegVal val)
    {
        setRegFlat(RegId(IntRegClass, idx), val);
    }

    RegVal
    readFloatRegFlat(RegIndex idx) const
    {
        return getRegFlat(RegId(FloatRegClass, idx));
    }
    void
    setFloatRegFlat(RegIndex idx, RegVal val)
    {
        setRegFlat(RegId(FloatRegClass, idx), val);
    }

    TheISA::VecRegContainer
    readVecRegFlat(RegIndex idx) const
    {
        TheISA::VecRegContainer val;
        getRegFlat(RegId(VecRegClass, idx), &val);
        return val;
    }
    TheISA::VecRegContainer&
    getWritableVecRegFlat(RegIndex idx)
    {
        return *(TheISA::VecRegContainer *)
            getWritableRegFlat(RegId(VecRegClass, idx));
    }
    void
    setVecRegFlat(RegIndex idx, const TheISA::VecRegContainer& val)
    {
        setRegFlat(RegId(VecRegClass, idx), &val);
    }

    RegVal
    readVecElemFlat(RegIndex idx) const
    {
        return getRegFlat(RegId(VecElemClass, idx));
    }
    void
    setVecElemFlat(RegIndex idx, RegVal val)
    {
        setRegFlat(RegId(VecElemClass, idx), val);
    }

    RegVal
    readCCRegFlat(RegIndex idx) const
    {
        return getRegFlat(RegId(CCRegClass, idx));
    }
    void
    setCCRegFlat(RegIndex idx, RegVal val)
    {
        setRegFlat(RegId(CCRegClass, idx), val);
    }
    /** @} */

    // hardware transactional memory
    virtual void htmAbortTransaction(uint64_t htm_uid,
                                     HtmFailureFaultCause cause) = 0;
    virtual BaseHTMCheckpointPtr& getHtmCheckpointPtr() = 0;
    virtual void setHtmCheckpointPtr(BaseHTMCheckpointPtr cpt) = 0;
};

/** @{ */
/**
 * Thread context serialization helpers
 *
 * These helper functions provide a way to the data in a
 * ThreadContext. They are provided as separate helper function since
 * implementing them as members of the ThreadContext interface would
 * be confusing when the ThreadContext is exported via a proxy.
 */

void serialize(const ThreadContext &tc, CheckpointOut &cp);
void unserialize(ThreadContext &tc, CheckpointIn &cp);

/** @} */


/**
 * Copy state between thread contexts in preparation for CPU handover.
 *
 * @note This method modifies the old thread contexts as well as the
 * new thread context. The old thread context will have its quiesce
 * event descheduled if it is scheduled and its status set to halted.
 *
 * @param new_tc Destination ThreadContext.
 * @param old_tc Source ThreadContext.
 */
void takeOverFrom(ThreadContext &new_tc, ThreadContext &old_tc);

} // namespace gem5

#endif
