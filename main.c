#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sgx_urts.h"
#include "stream_enclave_u.h"

#ifndef TSC_MHZ
#define TSC_MHZ 2112.0
#endif

char * sgx_error_to_string(sgx_status_t err);
uint64_t ocall_rdtsc();
double gettime();

int main() {
    int updated = 0;
    sgx_launch_token_t launch_token = {0};
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    sgx_enclave_id_t id = 0;
    double		start, create, stream, end;

    int fd;
    ssize_t num_bytes;
    char enclave[1024], token[1024];

    if (readlink ("/proc/self/exe", enclave, 1024) == -1)
    {
        printf("*** ERROR *** readlink(/proc/self/exe) failed\n");
        return -1;
    }
    dirname(enclave);
    strcpy(token, enclave);

    strcat(enclave, "/stream_enclave.signed.so");
    strcat(token, "/stream_enclave.token");

    fd = open(token, O_RDONLY);
    if (fd != -1) {
        num_bytes = read(fd, launch_token, sizeof(sgx_launch_token_t));
        close(fd);

        /* Ignore read failure, we intentionally don't check for a
         * non-existent file as stat and open cannot be run in parallel.
         */
        if (num_bytes != -1 && num_bytes != sizeof(sgx_launch_token_t)) {
            printf("*** ERROR *** cannot read launch token from: %s\n", token);
            return -2;
        }
    }

    start = gettime();
    
    /* Create the enclave.  Don't save the token so that the full create
     * flow is always benchmarked.
     */
    ret = sgx_create_enclave(enclave, SGX_DEBUG_FLAG, &launch_token, &updated, &id, NULL);
    if (ret != SGX_SUCCESS) {
        printf("*** ERROR *** sgx_create_enclave: %s\n", sgx_error_to_string(ret));
        return -3;
    }
    create = gettime();

    /* ECALL to the STREAM code */
    ret = ecall_Main_Loop(id);
    if (ret != SGX_SUCCESS) {
        printf("*** ERROR *** ecall_Main_Loop: %s\n", sgx_error_to_string(ret));
        return -4;
    }
    stream = gettime();

    /* Destroy the enclave */
    ret = sgx_destroy_enclave(id);
    if (ret != SGX_SUCCESS) {
        printf("*** ERROR *** sgx_destroy_enclave: %s\n", sgx_error_to_string(ret));
        return -5;
    }
    end = gettime();

    printf("Function    Time\n");
    printf("Create      %11.6f\n", create - start);
    printf("STREAM      %11.6f\n", stream - create);
    printf("Destroy     %11.6f\n", end - stream);
    printf("-------------------------------------------------------------\n");

    if (updated) {
        fd = creat(token, 0660);
        if (fd) {
            num_bytes = write(fd, launch_token, sizeof(sgx_launch_token_t));
            close(fd);

            if (num_bytes != sizeof(sgx_launch_token_t)) {
                printf("*** ERROR *** cannot save launch token to: %s\n", token);
                return -6;
            }
        }
    }
    return 0;
}

char * sgx_error_to_string(sgx_status_t err) {
    switch (err) {
    case SGX_ERROR_UNEXPECTED: return "Unexpected error occurred."; 
    case SGX_ERROR_INVALID_PARAMETER: return "Invalid parameter.";
    case SGX_ERROR_OUT_OF_MEMORY: return "Out of memory.";
    case SGX_ERROR_ENCLAVE_LOST: return "Power transition occurred.";
    case SGX_ERROR_INVALID_STATE: return "Invalid state";

    case SGX_ERROR_INVALID_FUNCTION: return "Invalid function";
    case SGX_ERROR_OUT_OF_TCS: return "Out of TCS";
    case SGX_ERROR_ENCLAVE_CRASHED: return "Enclave crashed";
    case SGX_ERROR_ECALL_NOT_ALLOWED: return "ECALL not allowed";
    case SGX_ERROR_OCALL_NOT_ALLOWED: return "OCALL not allowed";
    case SGX_ERROR_STACK_OVERRUN: return "Undefined symbol";

    case SGX_ERROR_UNDEFINED_SYMBOL: return "Undefined symbol";
    case SGX_ERROR_INVALID_ENCLAVE: return "Invalid enclave image";
    case SGX_ERROR_INVALID_ENCLAVE_ID: return "Invalid enclave identification";
    case SGX_ERROR_INVALID_SIGNATURE: return "Invalid enclave signature";
    case SGX_ERROR_NDEBUG_ENCLAVE: return "Enclave does not support debugging";
    case SGX_ERROR_OUT_OF_EPC: return "Out of EPC memory";
    case SGX_ERROR_NO_DEVICE: return "Invalid SGX device";
    case SGX_ERROR_MEMORY_MAP_CONFLICT: return "Memory map conflicted";
    case SGX_ERROR_INVALID_METADATA: return "Invalid enclave metadata";
    case SGX_ERROR_DEVICE_BUSY: return "SGX device was busy";
    case SGX_ERROR_INVALID_VERSION: return "Invalid enclave version";
    case SGX_ERROR_MODE_INCOMPATIBLE: return "Incompatible mode";
    case SGX_ERROR_ENCLAVE_FILE_ACCESS: return "Unable to open enclave file";
    case SGX_ERROR_INVALID_MISC: return "Invalid misc";

    case SGX_ERROR_MAC_MISMATCH: return "MAC mismatch";
    case SGX_ERROR_INVALID_ATTRIBUTE: return "Invalid enclave attribute";
    case SGX_ERROR_INVALID_CPUSVN: return "Invalid CPU SVN";
    case SGX_ERROR_INVALID_ISVSVN: return "Invalid ISV SVN";
    case SGX_ERROR_INVALID_KEYNAME: return "Invalid key name";

    case SGX_ERROR_SERVICE_UNAVAILABLE: return "Service unavailable";
    case SGX_ERROR_SERVICE_TIMEOUT: return "Service timeout";
    case SGX_ERROR_AE_INVALID_EPIDBLOB:  return "Invalid EPID blob";
    case SGX_ERROR_SERVICE_INVALID_PRIVILEGE:  return "Invalid privilege";
    case SGX_ERROR_EPID_MEMBER_REVOKED: return "EPID member revoked";
    case SGX_ERROR_UPDATE_NEEDED: return "Update needed";
    case SGX_ERROR_NETWORK_FAILURE: return "Network failure";
    case SGX_ERROR_AE_SESSION_INVALID: return "AE session invalid";
    case SGX_ERROR_BUSY: return "Busy.  What's busy you ask?  ...";
    case SGX_ERROR_MC_NOT_FOUND: return "MC not found";
    case SGX_ERROR_MC_NO_ACCESS_RIGHT: return "MC no access right";
    case SGX_ERROR_MC_USED_UP: return "MC used up";
    case SGX_ERROR_MC_OVER_QUOTA: return "MC over quota";
    case SGX_ERROR_KDF_MISMATCH: return "KDF mismatch";
    }
    return "Unknown error";
}

/*
 * ocall_print_string is an OCALL forwarding function for printf.
 * The enclave is responsible for validating str before calling
 * ocall_print_string.
 */
void ocall_print_string(const char *str)
{
    printf("%s", str);
}

uint64_t ocall_rdtsc()
{
    uint64_t rax, rdx;
    asm volatile("rdtsc" : "=a" (rax), "=d" (rdx));
    return (rdx << 32) + rax;
}

double gettime()
{
    return (((double)ocall_rdtsc() / (double)TSC_MHZ) * 1.e-6);
}
