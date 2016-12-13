CC = gcc

ifeq ($(SGX_DEBUG), 1)
	CFLAGS = -ggdb -DDEBUG -UNDEBUG -m64
else
	CFLAGS = -O2 -m64
endif

SGX_SDK ?= /opt/intel/sgxsdk
SGX_MODE ?= HW
SGX_ARCH ?= x64

SGX_LIBRARY_PATH = $(SGX_SDK)/lib64
SGX_ENCLAVE_SIGNER = $(SGX_SDK)/bin/x64/sgx_sign
SGX_EDGER8R = $(SGX_SDK)/bin/x64/sgx_edger8r

ifeq ($(SGX_MODE), SIM)
	SGX_TRTS = sgx_trts_sim
	SGX_URTS = sgx_urts_sim
else
	SGX_TRTS = sgx_trts
	SGX_URTS = sgx_urts
endif
SGX_CRYPTO = sgx_tcrypto

SGX_MAIN_CFLAGS =  -fPIC -Wno-attributes -I$(SGX_SDK)/include
SGX_ENCLAVE_CFLAGS = -nostdinc -fvisibility=hidden -fpie -I$(SGX_SDK)/include -I$(SGX_SDK)/include/tlibc

SGX_MAIN_LDFLAGS = -L$(SGX_LIBRARY_PATH) -l$(SGX_URTS) -lpthread
SGX_ENCLAVE_LDFLAGS = -Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -L$(SGX_LIBRARY_PATH) \
	-Wl,--whole-archive -l$(SGX_TRTS) -Wl,--no-whole-archive \
	-Wl,--start-group -lsgx_tstdc -l$(SGX_CRYPTO) -Wl,--end-group \
	-Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
	-Wl,-pie,-eenclave_entry -Wl,--export-dynamic  \
	-Wl,--defsym,__ImageBase=0 \
	-Wl,--version-script=stream_enclave.lds

#-lsgx_tstdcxx 
STREAM_ARRAY_SIZE=0x300000
STREAM_STACK_SIZE=0x4000
STREAM_HEAP_SIZE=$(shell echo $$(($(STREAM_ARRAY_SIZE)*8*3+0x1000)))

define STREAM_XML
<EnclaveConfiguration>
  <ProdID>0</ProdID>
  <ISVSVN>0</ISVSVN>
  <StackMaxSize>$(STREAM_STACK_SIZE)</StackMaxSize>
  <HeapMaxSize>$(STREAM_HEAP_SIZE)</HeapMaxSize>
  <TCSNum>1</TCSNum>
  <TCSPolicy>1</TCSPolicy>
  <DisableDebug>0</DisableDebug>
  <MiscSelect>0</MiscSelect>
  <MiscMask>0xFFFFFFFF</MiscMask>
</EnclaveConfiguration>
endef
export STREAM_XML

.PHONY: all clean

all: stream_sgx stream_enclave.signed.so

######## Untrusted ########
stream_enclave_u.c: $(SGX_EDGER8R) stream_enclave.edl
	$(SGX_EDGER8R) --untrusted stream_enclave.edl --search-path . --search-path $(SGX_SDK)/include

stream_sgx: main.c stream_enclave_u.c
	$(CC) $(CFLAGS) $(SGX_MAIN_CFLAGS) -o $@ $^ $(SGX_MAIN_LDFLAGS)

######## Enclave ########
stream_enclave_t.c: $(SGX_EDGER8R) stream_enclave.edl
	$(SGX_EDGER8R) --trusted stream_enclave.edl --search-path . --search-path $(SGX_SDK)/include

stream_enclave.so: stream.c stream_enclave_t.c
	$(CC) $(CFLAGS) $(SGX_ENCLAVE_CFLAGS) -DSTREAM_ARRAY_SIZE=$(STREAM_ARRAY_SIZE) $^ -o $@ $(SGX_ENCLAVE_LDFLAGS)

stream_enclave.signed.so: stream_enclave.so
	echo "$$STREAM_XML" > stream_enclave.xml
	$(SGX_ENCLAVE_SIGNER) sign -key stream_key.pem -enclave $^ -out $@ -config stream_enclave.xml

clean:
	rm -f stream_sgx *.o *.so stream_enclave_t.* stream_enclave_u.* stream_enclave.xml
