#!/bin/bash
#
# Simple run script to test QDMA in AXI-MM and AXI-St mode.
# 
# AXI-MM Transfer
#	First H2C operation is performed for 1KBytes, this will write 1Kbytes of data to BRAM on card side. 
#	Then C2H operation is performed for 1KBytes. DMA reads data from BRAM and will transfer
#	to local file 'out_mm0_0', which will be compared to original file for correctness. 


vf=0
qid=0
size=1024
num_qs=4



usr_bar=1 # User Control register space.

function queue_start() {
	echo "---- Queue Start $2 ----"
	dmactl qdma$1 q add idx $2 mode $3 dir h2c
	dmactl qdma$1 q start idx $2 dir h2c
	dmactl qdma$1 q add idx $2 mode $3 dir c2h
	dmactl qdma$1 q start idx $2 dir c2h
}

function cleanup_queue() {
	echo "---- Queue Clean up $2 ----"
        dmactl qdma$1 q stop idx $2 dir c2h
        dmactl qdma$1 q del idx $2 dir c2h
	dmactl qdma$1 q stop idx $2 dir h2c
        dmactl qdma$1 q del idx $2 dir h2c
}
echo "**** AXI-MM Start ****"

infile='/root/Desktop/datafile_16bit_pattern.bin'

for ((i=0; i< $num_qs; i++)) do
	# Setup for Queues
	qid=$i
	dev_mm_c2h="/dev/qdmavf0-MM-C2H-$qid"
	dev_mm_h2c="/dev/qdmavf0-MM-H2C-$qid"

	out_mm="out_mm0_"$qid
	# Open the Queue for AXI-MM streaming interface.
	queue_start vf$vf $qid mm

	# H2C transfer 
	dma_to_device -d $dev_mm_h2c -f $infile -s $size
	# C2H transfer
	dma_from_device -d $dev_mm_c2h -f $out_mm -s $size
	# Compare file for correctness
	cmp $out_mm $infile -n $size
	if [ $? -eq 1 ]; then
		echo "#### Test ERROR. Queue $qid data did not match ####"
		exit 1
	else
		echo "**** Test pass. Queue $qid"
	fi
	# Close the Queues
	cleanup_queue vf$vf $qid
done
echo "**** AXI-MM completed ****"




