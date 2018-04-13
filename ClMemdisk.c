/*
    This file is part of ClMemdisk.
    Copyright (C) 2017  ReimuNotMoe <reimuhatesfdt@gmail.com>

    ClMemdisk is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ClMemdisk is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ClMemdisk.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <CL/opencl.h>

#include <buse.h>

cl_context context;
cl_mem device_memory = NULL;
cl_command_queue commands;

void ShowHelp(){
	fprintf(stderr, "ClMemdisk v0.1 - An elegant way to make use of the spare memory of your OpenCL-compatible graphics card.\n"
		"Copyright (C) 2017 ReimuNotMoe <reimuhatesfdt@gmail.com>\n"
		"Licensed under GPLv3. See source distribution for detailed copyright notices.\n"
		"\n"
		"Usage: clmemdisk <operation> [/dev/nbdX] [size] [platform_id:device_id] [[platform_id:device_id]...]\n"
		"\n"
		"Operations:\n"
		"        list        List available OpenCL platforms and devices\n"
		"        single      Use memory from single device\n"
		"        stripe      Use memory from multiple devices with striping (WIP)\n"
		"\n"
		"Example:\n"
		"    # clmemdisk list\n"
		"    # clmemdisk single /dev/nbd0 512M 0:0\n"
		"    # clmemdisk stripe /dev/nbd233 1G 0:0 0:1 0:2\n");
}

static int MemRead(void *buf, uint32_t len, uint64_t offset, void *userdata){

	void *gpubuf;

	fprintf(stderr, "Debug: Read request at %"PRIu64", length %"PRIu32".\n", offset, len);

	clEnqueueReadBuffer(commands, device_memory, CL_TRUE, (size_t)offset, (size_t)len, buf, 0, NULL, NULL);
	clFinish(commands);

	return 0;
}

static int MemWrite(const void *buf, uint32_t len, uint64_t offset, void *userdata){

	void *gpubuf;

	fprintf(stderr, "Debug: Write request at %"PRIu64", length %"PRIu32".\n", offset, len);

	clEnqueueWriteBuffer(commands, device_memory, CL_TRUE, (size_t)offset, (size_t)len, buf, 0, NULL, NULL);
	clFinish(commands);

	return 0;
}

static void Disconnect(void *userdata){
	fprintf(stderr, "Received a disconnect request.\n");
	clReleaseMemObject(device_memory);
	clReleaseCommandQueue(commands);
	clReleaseContext(context);
}

static int MemFlush(void *userdata){
	fprintf(stderr, "Received a flush request.\n");
	return 0;
}

static int MemTrim(uint64_t from, uint32_t len, void *userdata){
	fprintf(stderr, "Received a flush request at %"PRIu64", length %"PRIu32".\n", from, len);
	return 0;
}

int main(int argc, char** argv){
	if (argc == 1) {
		ShowHelp();
		exit(1);
	}

	int opmode = 0;
	size_t mem_size_reqd = 0;
	size_t device_id_reqd[2];

	if (0 == strcmp(argv[1], "list")) {

	} else if (0 == strcmp(argv[1], "single")) {
		opmode = 1;

	} else if (0 == strcmp(argv[1], "stripe")) {
		opmode = 2;

	} else {
		ShowHelp();
		exit(1);
	}

	if (opmode > 0) {
		if (argc < 5) {
			ShowHelp();
			exit(1);
		}
		mem_size_reqd = strtoul(argv[3], NULL, 10);

		if ( strchr(argv[3], 'K') )
			mem_size_reqd *= 1024;
		else if ( strchr(argv[3], 'M') )
			mem_size_reqd *= 1024 * 1024;
		else if ( strchr(argv[3], 'G') )
			mem_size_reqd *= 1024 * 1024 * 1024;
		else if ( strchr(argv[3], 'T') )
			mem_size_reqd *= 1024 * 1024 * 1024; // Really bro? Really?!

		char *delim = strchr(argv[4], ':');

		if (!delim) {
			fprintf(stderr, "Error: Bad PlatformID:DeviceID combination: %s\n", argv[3]);
			exit(2);
		}

		*delim = 0;

		device_id_reqd[0] = strtoul(argv[4], NULL, 10);
		device_id_reqd[1] = strtoul(delim+1, NULL, 10);

		fprintf(stderr, "Info: Enqueuing device %lu:%lu\n", device_id_reqd[0], device_id_reqd[1]);
	}

	int err;

	cl_platform_id platform_ids[16];
	cl_uint nr_platform_ids = 0;

	cl_device_id device_ids[16][16];
	cl_uint nr_device_ids[16] = {0};

	char device_name[128] = {0};
	char device_vendor[128] = {0};

	cl_ulong device_availmem = 0;
	cl_bool device_usable = 0;

	clGetPlatformIDs(16, platform_ids, &nr_platform_ids);

	if (!nr_platform_ids) {
		fprintf(stderr, "Error: no compatible platform found\n");
		exit(2);
	}

	if (opmode == 0) {
		fprintf(stderr, "Info: %u compatible platform(s) found\n", nr_platform_ids);
		fprintf(stderr, "Available devices:\n"
			"Platform ID  Device ID  Name                   Vendor                    Total Memory      Usable\n"
			"=================================================================================================\n");
	}

	for (int j=0; j<nr_platform_ids; j++){
		clGetDeviceIDs(platform_ids[j], CL_DEVICE_TYPE_ALL, sizeof(device_ids)/sizeof(cl_device_id), device_ids[j], &nr_device_ids[j]);

		if (opmode == 0)
			for (int h=0; h<nr_device_ids[j]; h++){
				clGetDeviceInfo(device_ids[j][h], CL_DEVICE_NAME, sizeof(device_name)-1, device_name, NULL);
				clGetDeviceInfo(device_ids[j][h], CL_DEVICE_VENDOR, sizeof(device_vendor)-1, device_vendor, NULL);
				clGetDeviceInfo(device_ids[j][h], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(device_availmem), &device_availmem, NULL);
				clGetDeviceInfo(device_ids[j][h], CL_DEVICE_AVAILABLE, sizeof(device_usable), &device_usable, NULL);

				fprintf(stderr, "%11d  %9d  %15s  %20s  %16lu  %7d\n", j, h, device_name, device_vendor, device_availmem, device_usable);
			}
	}

	if (opmode == 0)
		exit(0);


	cl_device_id device_id = device_ids[device_id_reqd[0]][device_id_reqd[1]];             // compute device id


	// Create a compute context

	context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);

	if (!context) {
		printf("Error: Failed to create a compute context!\n");
		return EXIT_FAILURE;
	}

	commands = clCreateCommandQueue(context, device_id, 0, &err);
	if (!commands) {
		printf("Error: Failed to create a command commands!\n");
		return EXIT_FAILURE;
	}

	fprintf(stderr, "Info: Allocating %zu bytes memory on device %zu:%zu\n", mem_size_reqd, device_id_reqd[0], device_id_reqd[1]);

	device_memory = clCreateBuffer(context, CL_MEM_READ_WRITE, mem_size_reqd, NULL, NULL);

	if (!device_memory) {
		printf("Error: Failed to allocate device memory!\n");
		exit(2);
	}

	struct buse_operations buseops;

	buseops.read = MemRead;
	buseops.write = MemWrite;
	buseops.disc = Disconnect;
	buseops.flush = MemFlush;
	buseops.trim = MemTrim;
	buseops.size = (u_int64_t)mem_size_reqd;

	return buse_main(argv[2], &buseops, NULL);
}