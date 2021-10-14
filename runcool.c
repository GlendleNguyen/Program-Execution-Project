//  CITS2002 Project 1 2021
//  Name(s):             Cathy Ngo, Glendle Nguyen
//  Student number(s):   22712411, 22575354

//  compile with:  cc -std=c11 -Wall -Werror -o runcool runcool.c

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//  THE STACK-BASED MACHINE HAS 2^16 (= 65,536) WORDS OF MAIN MEMORY
#define N_MAIN_MEMORY_WORDS (1<<16)

//  EACH WORD OF MEMORY CAN STORE A 16-bit UNSIGNED ADDRESS (0 to 65535)
#define AWORD               uint16_t
//  OR STORE A 16-bit SIGNED INTEGER (-32,768 to 32,767)
#define IWORD               int16_t

//  THE ARRAY OF 65,536 WORDS OF MAIN MEMORY
AWORD                       main_memory[N_MAIN_MEMORY_WORDS];

//  THE SMALL-BUT-FAST CACHE HAS 32 WORDS OF MEMORY
#define N_CACHE_WORDS       32


//  see:  http://teaching.csse.uwa.edu.au/units/CITS2002/projects/coolinstructions.php
enum INSTRUCTION {
    I_HALT       = 0,
    I_NOP,
    I_ADD,
    I_SUB,
    I_MULT,
    I_DIV,
    I_CALL,
    I_RETURN,
    I_JMP,
    I_JEQ,
    I_PRINTI,
    I_PRINTS,
    I_PUSHC,
    I_PUSHA,
    I_PUSHR,
    I_POPA,
    I_POPR
};

//  USE VALUES OF enum INSTRUCTION TO INDEX THE INSTRUCTION_name[] ARRAY
const char *INSTRUCTION_name[] = {
    "halt",
    "nop",
    "add",
    "sub",
    "mult",
    "div",
    "call",
    "return",
    "jmp",
    "jeq",
    "printi",
    "prints",
    "pushc",
    "pusha",
    "pushr",
    "popa",
    "popr"
};

//  ----  IT IS SAFE TO MODIFY ANYTHING BELOW THIS LINE  --------------

//  THE STATISTICS TO BE ACCUMULATED AND REPORTED
int n_main_memory_reads     = 0;
int n_main_memory_writes    = 0;
int n_cache_memory_hits     = 0;
int n_cache_memory_misses   = 0;

void report_statistics(void)
{
    printf("@number-of-main-memory-reads\t%i\n",    n_main_memory_reads);
    printf("@number-of-main-memory-writes\t%i\n",   n_main_memory_writes);
    printf("@number-of-cache-memory-hits\t%i\n",    n_cache_memory_hits);
    printf("@number-of-cache-memory-misses\t%i\n",  n_cache_memory_misses);
}

//  -------------------------------------------------------------------

struct {
    // 0 = valid, 1 = invalid line 
    int valid; 
    // 0 = clean, 1 = dirty data                          
    int dirty;          
    // tag gives a direct map to main memory address                
    AWORD tag;
    // data contains word to be processed                           
    AWORD data;                        
    
} cache[N_CACHE_WORDS];

// MARK ALL CACHE LINES AS VALID UPON INITIALISING
void initialise_cache() {
    for (int i=0; i<N_CACHE_WORDS; ++i) {
        cache[i].valid = 0;
    }
}

//  -------------------------------------------------------------------

//  EVEN THOUGH main_memory[] IS AN ARRAY OF WORDS, IT SHOULD NOT BE ACCESSED DIRECTLY.
//  INSTEAD, USE THESE FUNCTIONS read_memory() and write_memory()
//
//  THIS WILL MAKE THINGS EASIER WHEN WHEN EXTENDING THE CODE TO
//  SUPPORT CACHE MEMORY

AWORD read_memory(int address)
{
    int cache_line = address % 32; 
    int new_tag = address / 32;    

    if(cache[cache_line].valid == 0) {                                                  // If invalid then set as valid
        cache[cache_line].valid = 1; 
    } 
    else {                                                                              // [cache hit]
        if (cache[cache_line].tag == new_tag) {                                         // If valid and the address matches cache tag
            ++n_cache_memory_hits;                                                      // Increment cache hit
            return cache[cache_line].data;                                              // Data can be directly returned from the cache
        }

        else {                                                                          // Else, if the address does not match
            if(cache[cache_line].dirty == 1) {                                          // Write existing data to main memory if dirty
                int wr_address = 32 * cache[cache_line].tag + cache_line;
                main_memory[wr_address] = cache[cache_line].data; 
                ++n_main_memory_writes;               
            }
        }
    }

    cache[cache_line].data = main_memory[address];                                      // [cache miss] 
    ++n_main_memory_reads;                                                              // Pop into cache and increment metrics
    ++n_cache_memory_misses;                                                            
    cache[cache_line].dirty = 0;                                                        // Set cache line to clean     
    cache[cache_line].tag = new_tag;                                                    // Set new tag 
   
    return cache[cache_line].data;                                                      // Return the requested data at the desired address
}

void write_memory(AWORD address, AWORD value)
{   
    int cache_line = (address % 32);                                                    // Find matching cache address
    int new_tag = address / 32;                                                         // Set new tag for cache line
    
    if(cache[cache_line].valid == 1) {                                                  // Check if existing cache line valid
        if(cache[cache_line].dirty == 1) {                                              // Check if existing cache line dirty 
            if(cache[cache_line].tag != new_tag) {                                      // Check if cache line tag points to same location in main memory 
                int main_memory_address = (cache[cache_line].tag * 32) + cache_line;    // Write existing data to main memory
                main_memory[main_memory_address] = cache[cache_line].data;
                ++n_main_memory_writes;
            }
        } else {
            cache[cache_line].dirty = 1;                                                // Set cache line as dirty 
        }
    } else {                                                                            // Set cache line as dirty and valid 
        cache[cache_line].dirty = 1;
        cache[cache_line].valid = 1;
    }
    cache[cache_line].data = value;                                                     // Write value to cache line and set new tag
    cache[cache_line].tag = new_tag;
}

//  -------------------------------------------------------------------

//  EXECUTE THE INSTRUCTIONS IN main_memory[]
int execute_stackmachine(void)
{
//  THE 3 ON-CPU CONTROL REGISTERS:
    int PC      = 0;                    // 1st instruction is at address=0
    int SP      = N_MAIN_MEMORY_WORDS;  // initialised to top-of-stack
    int FP      = 0;                    // frame pointer

//  REMOVE THE FOLLOWING LINE ONCE YOU ACTUALLY NEED TO USE FP
//  FP = FP+0;

    while(true) {
        IWORD       value1;
        IWORD       value2;
        IWORD       value3;
        
//  FETCH THE NEXT INSTRUCTION TO BE EXECUTED

        IWORD instruction   = read_memory(PC);
        ++PC;

        if(instruction == I_HALT) {
            break;
        }

//  SUPPORT OTHER INSTRUCTIONS HERE

        switch (instruction) {

        case I_NOP :
        /* -------------------- NO OPERATION -------------------- */
        /*       THE PC IS ADVANCED TO THE NEXT INSTRUCTION       */
        /* ------------------------------------------------------ */
                            break;

        case I_ADD :
        /* ---------------------- ADDITION ---------------------- */
        /*    THE TWO INTEGERS ON THE TOS ARE POPPED FROM THE     */
        /*  STACK ADDED TOGETHER, AND THE RESULT LEFT ON THE TOS  */
        /* ------------------------------------------------------ */
                            value1 = read_memory(SP);                               
                            ++SP;
                            value2 = read_memory(SP);                               
                            write_memory(SP, value2 + value1);                      
                            break;

        case I_SUB :
        /* ------------------- SUBTRACTION ---------------------- */
        /*     THE TWO INTEGERS ON THE TOS ARE POPPED FROM THE    */ 
        /*    STACK, THE SECOND SUBTRACTED FROM THE FIRST, AND    */
        /*           THE FIRST LEFT ON TOP OF THE STACK           */
        /* ------------------------------------------------------ */     
                            value1 = read_memory(SP);                              
                            ++SP;
                            value2 = read_memory(SP);                               
                            write_memory(SP, value2 - value1);                      
                            break;



        case I_MULT :
        /* ------------------ MULTIPLICATION ------------------- */
        /* THE TWO INTEGERS ON THE TOS ARE POPPED FROM THE STACK */
        /*  MULTIPLIED TOGETHER, AND THE RESULT LEFT ON THE TOS  */        
        /* ----------------------------------------------------- */
                            value1 = read_memory(SP);                               
                            ++SP;
                            value2 = read_memory(SP);                               
                            write_memory(SP, value2 * value1);                      
                            break;

        case I_DIV :
        /* --------------------- DIVISION ---------------------- */
        /* THE TWO INTEGERS ON THE TOS ARE POPPED FROM THE STACK */
        /*  MULTIPLIED TOGETHER, AND THE RESULT LEFT ON THE TOS  */        
        /* ----------------------------------------------------- */        
                            value1 = read_memory(SP);                              
                            ++SP;
                            value2 = read_memory(SP);                               
                            write_memory(SP, value2 / value1);                     
                            break;

        case I_CALL : 
        /* ----------------------- FUNCTION CALL ------------------------- */
        /*  THE WORD FOLLOWING THE 'CALL' INSTRUCTION HOLDS THE ADDRESS OF */
        /* THE REQUIRED FUNCTION'S FIRST INSTRUCTION. THE FUNCTION'S FIRST */
        /*       INSTRUCTION IS THE NEXT INSTRUCTION TO BE EXECUTED        */                    
        /* --------------------------------------------------------------- */  
                            --SP;                                                   // Move to next stack position
                            write_memory(SP, PC+1);                                 // Save the return address on stack
                            value1 = read_memory(PC);                               // Save address of required function's first instruction
                            PC = value1;                                            // Move PC to function's first function to be executed
                            --SP;                                                   // Move to the next stack position
                            write_memory(SP, FP);                                   // Save frame pointer position on stack
                            FP = SP;                                                // Move to new frame pointer

                            break;    
                            
        case I_RETURN : 
        /* ----------------------- FUNCTION RETURN ------------------------------ */
        /* THE FLOW OF EXECUTION RETURNS TO THE INSTRUCTION IMMEDIATELY FOLLOWING */
        /* THE 'CALL' INSTRUCTION THAT CALLED THE CURRENT FUNCTION. THE INTEGER   */
        /* VALUE ON THE TOS IS THE FUNCTION'S RETURNED VALUE (EVEN IF THE CURRENT */
        /* FUNCTION IS OF TYPE VOID). THE VALUE IMMEDIATELY FOLLOWING THE RETURN  */
        /* OPCODE INDICATES WHERE THE FUNCTION'S RETURNED VALUE SHOULD BE COPIED  */
        /* BEFORE THE FUNCTION REUTRNS - TO THE LOCATION OF FP+VALUE OF THE       */
        /*                  CURRENT FUNCTION'S STACK FRAME.                       */  
        /* ---------------------------------------------------------------------- */  
                            value1 = read_memory(PC);                               // Save offset
                            value2 = read_memory(FP+1);                             // Save return address
                            value3 = read_memory(SP);                               // Save returned value
                            SP = FP + value1;                                       // Set SP to FP + offset
                            write_memory(SP, value3);                               // Write return value to stack
                            FP = read_memory(FP);                                   // Set FP back to previous FP
                            PC = value2;                                            // Move PC to return address

                            break;

        case I_JMP : 
        /* ----------------------- UNCONDITIONAL JUMP ------------------------- */
        /*      THE FLOW OF EXECUTION CONTINUES AT THE ADDRESS IN THE WORD      */
        /*            IMMEDIATELY FOLLOWING THE 'JMP' INSTRUCTION               */                   
        /* -------------------------------------------------------------------- */         
                            value1 = read_memory(PC);                               // Save address to be 'jumped to'
                            PC = value1;                                            // Move PC to this address

                            break;

        case I_JEQ : 
        /* ------------------------ CONDITIONAL JUMP -------------------------- */
        /*  THE VALUE ON THE TOS IS POPPED FROM THE STACK AND, IFF THE VALUE IS */
        /*    ZERO THE FLOW OF EXECUTION CONTINUES AT THE ADDRESS IN THE WORD   */       
        /*             IMMEDIATELY FOLLOWING THE 'JEQ' INSTRUCTION              */                           
        /* -------------------------------------------------------------------- */  
                            value1 = read_memory(SP);                               // Hold value on top of stack, then pop
                            ++SP;                             

                            if (value1 == 0) {                                      // Iff the value is zero then alter flow of execution
                                value2 = read_memory(PC);                    
                                PC = value2;                                        // Move PC to given address
                            }
                            else {                                                  // Else increment PC as normal
                                ++PC;                        
                            }

                            break;
                            
        case I_PRINTI : 
        /* ----------------------------PRINT INTEGER -------------------------- */
        /* THE VALUE ON THE TOS IS POPPED FROM THE STACK AND PRINTED TO STDOUT  */           
        /* -------------------------------------------------------------------- */     
                            value1 = read_memory(SP);                               // Pop value on the TOS
                            ++SP;
                            printf("%d", value1);                                   // Print to stdout

                            break;

        case I_PRINTS :
        /* -------------------------- PRINT STRING --------------------------- */
        /* THE NULL-BYTE TERMINATED CHARACTER STRING WHOSE ADDRESS IMMEDIATELY */
        /*        FOLLOWS THE 'PRINTS' INSTRUCTION IS PRINTED TO STDOUT        */                                
        /* ------------------------------------------------------------------- */  
                            value1 = read_memory(PC);                               // Read in current line
                            ++PC;

                            while(true){
                                AWORD chars = read_memory(value1);     
                                char xlow = chars & 0xff;                           // 0000000011111111 bitmasking                
                                if(xlow == '\0') {                                  // E.g. xlow = 00100000 compared to nul '\0'
                                    break;                             
                                    } else {                           
                                    printf("%c", xlow);                             // Print xlow character (e.g. 00100000)
                                    }                                  
                                char xhigh = chars >> 8;                            // Bitshifting                              
                                if(xhigh == '\0') {                                 // E.g. xhigh = 01100110 compared to nul '\0'
                                    break;                             
                                } else {                               
                                    printf("%c", xhigh);                            // Print xhigh character (e.g. 01100110)
                                    }  
                                value1++;                                           // Read next address
                            }

                            break;           
                            
        case I_PUSHC : 
        /* --------------------- PUSH INTEGER CONSTANT ----------------------- */
        /*     THE INTEGER CONSTANT IN THE WORD IMMEDIATELY FOLLOWING THE      */
        /*            'PUSHC' INSTRUCTION IS PUSHED ONTO THE STACK             */                                
        /* ------------------------------------------------------------------- */                   
                            value1 = read_memory(PC);                               // Read in constant to be push
                            ++PC;
                            --SP;
                            write_memory(SP, value1);                               // Push constant onto TOS
                            break;

        case I_PUSHA : 
        /* ------------------------ PUSH ABSOLUTE -------------------------- */
        /*  THE WORD IMMEDIATELY FOLLOWING THE 'PUSHA' INSTRUCTION HOLDS THE */
        /*     ADDRESS OF THE INTEGER VALUE TO BE PUSHED ONTO THE STACK      */                                 
        /* ----------------------------------------------------------------- */ 
                            value1 = read_memory(PC);                               // Save address of integer to be pushed on stack
                            --SP;
                            write_memory(SP, read_memory(value1));                  // Push value at the address onto stack
                            ++PC;                            
                            break;
                            
        case I_PUSHR : // PUSH RELATIVE
        /* ------------------------ PUSH RELATIVE -------------------------- */
        /*      THE WORD IMMEDIATELY FOLLOWING THE 'PUSHR' INSTRUCTION       */
        /*     PROVIDES THE OFFSET TO BE ADDED TO THE FP TO PROVIDE THE      */  
        /*              ADDRESS OF THE INTEGER VALUE TO BE PUSHED            */                                        
        /* ----------------------------------------------------------------- */               
                            value1 = read_memory(PC);                               // Save the offset to be added to FP
                            value2 = read_memory(FP + value1);                      // Holds integer from address
                            --SP;
                            write_memory(SP, value2);                               // Push this integer value to TOS
                            ++PC;
                            break;

        case I_POPA :
        /* ------------------------ POP ABSOLUTE -------------------------- */
        /*  THE WORD IMMEDIATELY FOLLOWING THE 'POPA' INSTRUCTION HOLDS THE */
        /*     ADDRESS INTO WHICH THE VALUE ON THE TOS SHOULD BE POPPED     */                              
        /* ---------------------------------------------------------------- */ 
                            value1 = read_memory(SP);                               // Current value on TOS
                            value2 = read_memory(PC);                               // Address into which value should be popped
                            ++SP;
                            write_memory(value2, value1);                           // Poppinig value on TOS to desired address
                            ++PC;                                                                    
                            break;

        case I_POPR :
        /* ------------------------ POP RELATIVE -------------------------- */
        /*       THE WORD IMMEDIATELY FOLLOWING THE 'POPR' INSTRUCTION      */
        /*     PROVIDES THE OFFSET TO BE ADDED TO THE FP TO PROVIDE THE     */ 
        /*     ADDRESS WHICH THE VALUE ON THE TOS SHOULD BE POPPED INTO     */                       
        /* ---------------------------------------------------------------- */ 
                            value1 = read_memory(PC);                               // Gives the offset to be added to FP
                            value2 = FP + value1;                                   // Holds the address to which value will be popped
                            value3 = read_memory(SP);                               // Pop current top of stack
                            ++SP;                                          
                            write_memory(value2, value3);                   
                            ++PC;
                            
                            break;
        }
    }
//  THE RESULT OF EXECUTING THE INSTRUCTIONS IS FOUND ON THE TOP-OF-STACK
     return read_memory(SP);
 }

//  -------------------------------------------------------------------


//  READ THE PROVIDED coolexe FILE INTO main_memory[]
void read_coolexe_file(char filename[])
{
    memset(main_memory, 0, sizeof main_memory);   

 // READ CONTENTS OF coolexe FILE
    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        printf("Cannot open file '%s'\n.", filename);
        exit(EXIT_FAILURE);
    }

    fread(main_memory, sizeof (AWORD), N_MAIN_MEMORY_WORDS, fp);

    fclose(fp); 
}

//  -------------------------------------------------------------------


int main(int argc, char *argv[])
{
//  CHECK THE NUMBER OF ARGUMENTS
    if(argc != 2) {
        fprintf(stderr, "Usage: %s program.coolexe\n", argv[0]);
        exit(EXIT_FAILURE);
    }

//  INITIALISE THE CACHE
    initialise_cache();

//  READ THE PROVIDED coolexe FILE INTO THE EMULATED MEMORY
    read_coolexe_file(argv[1]);

//  EXECUTE THE INSTRUCTIONS FOUND IN main_memory[]
    int result = execute_stackmachine();

    report_statistics();
        printf("@exit(%i)\n", result);

    return result; 
}