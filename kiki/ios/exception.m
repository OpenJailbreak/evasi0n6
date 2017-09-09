#import <Foundation/Foundation.h>

#include <mach/mach.h>
#include <mach/exc.h>
#include <mach/thread_status.h>
#include <stdio.h>
#include <stdlib.h>
#include <launch.h>

//#define START_MS_WHEN_LOADED

extern boolean_t exc_server(mach_msg_header_t *, mach_msg_header_t *);

#ifdef START_MS_WHEN_LOADED
#include <pthread.h>

void start_ms_function(void* arg)
{
    system("/usr/bin/cynject 1 /Library/Frameworks/CydiaSubstrate.framework/Libraries/SubstrateLauncher.dylib");
    system("killall SpringBoard");
}
#endif

int main(int argc, char* argv[])
{
    mach_port_t exc;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &exc);
    mach_port_insert_right(mach_task_self(), exc, exc, MACH_MSG_TYPE_MAKE_SEND);

#ifdef START_MS_WHEN_LOADED
    pthread_t start_ms_thread;
    pthread_create(&start_ms_thread, NULL, (void*)start_ms_function, NULL);
#endif

    NSLog(@"host_set_exception_ports = %d", host_set_exception_ports(mach_host_self(), EXC_MASK_BAD_INSTRUCTION, exc, EXCEPTION_STATE_IDENTITY, ARM_THREAD_STATE));

    while(1)
        mach_msg_server_once(exc_server, sizeof(__Reply__exception_raise_state_identity_t), exc, 0);

    return 0;
}

kern_return_t task_read(task_t task, uint32_t address, void* buffer, size_t size)
{
    vm_size_t inSize = size;
    return vm_read_overwrite(task, (mach_vm_address_t) address, inSize, (mach_vm_address_t) buffer, &inSize);
}

kern_return_t task_write(task_t task, uint32_t address, void* buffer, size_t size)
{
    return vm_write(task, (mach_vm_address_t) address, (mach_vm_address_t) buffer, size);
}

kern_return_t catch_exception_raise_state_identity(
        mach_port_t exception_port,
        mach_port_t thread,
        mach_port_t task,
        exception_type_t exception,
        exception_data_t code,
        mach_msg_type_number_t codeCnt,
        int *flavor,
        thread_state_t old_state,
        mach_msg_type_number_t old_stateCnt,
        thread_state_t new_state,
        mach_msg_type_number_t *new_stateCnt)
{
    arm_thread_state_t* arm_old_state = (arm_thread_state_t*) old_state;

    arm_thread_state_t* arm_new_state = (arm_thread_state_t*) new_state;
    *arm_new_state = *arm_old_state;
    *new_stateCnt = sizeof(*arm_new_state);

    BOOL thumb = ((arm_old_state->__cpsr & (1 << 5)) == (1 << 5)) ? YES : NO;
    BOOL N = ((arm_old_state->__cpsr & (1 << 31)) == (1 << 31)) ? YES : NO;
    BOOL Z = ((arm_old_state->__cpsr & (1 << 30)) == (1 << 30)) ? YES : NO;
    BOOL C = ((arm_old_state->__cpsr & (1 << 29)) == (1 << 29)) ? YES : NO;
    BOOL V = ((arm_old_state->__cpsr & (1 << 28)) == (1 << 28)) ? YES : NO;


    uint32_t armInsn;
    uint16_t thumbInsn;
    uint32_t insn;

    task_read(task, arm_old_state->__pc, thumb ? (void*)&thumbInsn : (void*)&armInsn, thumb ? sizeof(thumbInsn) : sizeof(armInsn));
    insn = thumb ? thumbInsn : armInsn;

    //NSLog(@"Got exception. pc = 0x%08x, thumb = %d, insn = 0x%x", arm_old_state->__pc, thumb, insn);

    if(!thumb)
    {
        uint8_t condition = (insn >> 28) & 0xF;
        BOOL execute;

        switch(condition)
        {
            case 0:
                execute = Z ? YES : NO;
                break;
            case 1:
                execute = Z ? NO : YES;
                break;
            case 2:
                execute = C ? YES : NO;
                break;
            case 3:
                execute = C ? NO : YES;
                break;
            case 4:
                execute = N ? YES : NO;
                break;
            case 5:
                execute = N ? NO : YES;
                break;
            case 6:
                execute = V ? YES : NO;
                break;
            case 7:
                execute = V ? NO : YES;
                break;
            case 8:
                execute = (C && !Z) ? YES : NO;
                break;
            case 9:
                execute = (!C || Z) ? YES : NO;
                break;
            case 10:
                execute = ((N && V) || (!N && !V)) ? YES : NO;
                break;
            case 11:
                execute = ((N && !V) || (!N && V)) ? YES : NO;
                break;
            case 12:
                execute = (!Z && ((N && V) || (!N && !V))) ? YES : NO;
                break;
            case 13:
                execute = (Z || (N && !V) || (!N && V)) ? YES : NO;
                break;
            case 14:
                execute = YES;
                break;
            case 15:
                execute = NO;
                break;
        }

        if(execute)
        {
            if(((insn >> 26) & 3) == 1)
            {
                // Single data transfer
                //NSLog(@"Handling single data transfer instruction!");

                BOOL immediate = ((insn & (1 << 25)) == (1 << 25)) ? NO : YES;
                BOOL postIndexed = ((insn & (1 << 24)) == (1 << 24)) ? NO : YES;
                BOOL upOffset = ((insn & (1 << 23)) == (1 << 23)) ? YES : NO;
                BOOL transferByte = ((insn & (1 << 22)) == (1 << 22)) ? YES : NO;
                BOOL writeBack = ((insn & (1 << 21)) == (1 << 21)) ? YES : NO;
                BOOL isLoad = ((insn & (1 << 20)) == (1 << 20)) ? YES : NO;

                uint8_t baseRegister = (insn >> 16) & 0xF;
                uint8_t operandRegister = (insn >> 12) & 0xF;
                int8_t shift = immediate ? 0 : ((insn >> 4) & 0xFF);

                int32_t offset;
                if(immediate)
                {
                    offset = insn & 0xFFF;
                } else
                {
                    // when accessing __r, we don't need to worry about the r[13-15] cases, since the structure makes them synonymous to sp, lr, pc.
                    if(shift > 0)
                        offset = arm_old_state->__r[insn & 0xF] << shift;
                    else if(shift < 0)
                        offset = arm_old_state->__r[insn & 0xF] >> -shift;
                    else
                        offset = arm_old_state->__r[insn & 0xF];
                }

                uint32_t address = arm_old_state->__r[baseRegister];
                if(!postIndexed)
                    address += upOffset ? offset : -offset;

                if(isLoad)
                {
                    if(transferByte)
                    {
                        uint8_t v;
                        task_read(task, address, &v, sizeof(v));
                        arm_new_state->__r[operandRegister] = v;
                    } else
                    {
                        uint32_t v;
                        task_read(task, address, &v, sizeof(v));
                        arm_new_state->__r[operandRegister] = v;
                    }
                } else
                {
                    if(transferByte)
                    {
                        uint8_t v = arm_new_state->__r[operandRegister];
                        task_write(task, address, &v, sizeof(v));
                    } else
                    {
                        uint32_t v = arm_new_state->__r[operandRegister];
                        task_write(task, address, &v, sizeof(v));
                    }
                }

                if(postIndexed)
                    address += upOffset ? offset : -offset;

                if(writeBack)
                    arm_old_state->__r[baseRegister] = address;
            } else
            {
                //NSLog(@"Skipping unknown ARM instruction!");
            }
        } else
        {
            //NSLog(@"Skipping ARM instruction due to condition codes.");
        }
    } else
    {
        //NSLog(@"Skipping unknown THUMB instruction!");
    }

    arm_new_state->__pc += thumb ? 2 : 4;

    return KERN_SUCCESS;
}

