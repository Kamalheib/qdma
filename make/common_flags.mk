#Build flags common to xdma/qdma

ifeq ($(VF),1)
   EXTRA_FLAGS = -D__QDMA_VF__
   PFVF_TYPE = _vf
else
   PFVF_TYPE = 
endif

