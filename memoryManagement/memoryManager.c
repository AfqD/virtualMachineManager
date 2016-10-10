#include<stdio.h>
#include<libvirt/libvirt.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<time.h>
#include<stdbool.h>
#include<math.h>


struct DomainMemory {
	virDomainPtr dom;
	unsigned long long int unusedMemory;
	unsigned long long int availableMemory;
	unsigned long long int workingSetSize;
	unsigned long long int balloonSize;
	unsigned long int maxMemory;
};

struct DomainMemory returnDomainMemoryStruct(virDomainPtr dom);

int main(int argc, char* argv[]) {

	while(1) {

		clock_t begin;
		double time_spent;
		unsigned int i=0;
		begin=clock();

		for(i=0;1;i++) {
			int n = atoi(argv[1]);
			int sleeptime = n-1;
			printf("sleeptime =%d\n",sleeptime);
			
			// connecting to qemu hypervisor
			virConnectPtr conn = virConnectOpen("qemu:///system");
			if(conn==NULL) {
        			fprintf(stderr,"Failed to connect to qemu:///system\n");
        			return 1;
			}
			
			virNodeMemoryStatsPtr params;
			int cellNum =-1;
			int nparams =0;

			if (virNodeGetMemoryStats(conn, cellNum, NULL, &nparams, 0) == 0 && nparams != 0) {
    				if ((params = malloc(sizeof(virNodeMemoryStats) * nparams)) != NULL) {
					memset(params, 0, sizeof(virNodeMemoryStats) * nparams);
    					if (virNodeGetMemoryStats(conn, cellNum, params, &nparams, 0)) {
						printf("error\n");
					}
				}
			}	
			
			int count =0;
			while(count<nparams) {
				printf("node memory %s - %llu\n",params->field,params->value);	
				params++;
				count++;
			}
			int j=0;
			int numActiveDomains=0;
			numActiveDomains = virConnectNumOfDomains(conn);	
			printf("numactivedomains = %d\n",numActiveDomains);
			
			virDomainPtr* allDomains;
			allDomains = malloc(sizeof(virDomainPtr)*numActiveDomains);

			int* activeDomains;
			activeDomains = malloc(sizeof(int) * numActiveDomains);
			
			virConnectListDomains(conn,activeDomains,numActiveDomains);

			struct DomainMemory domainMemory[numActiveDomains];
			
			sleep(sleeptime);
			
			for (j = 0 ; j < numActiveDomains ; j++) {
			        allDomains[j] = virDomainLookupByID(conn, activeDomains[j]);
			        printf("\nActive - %d, %s, Id: %d\n", j+1,virDomainGetName(allDomains[j]), virDomainGetID(allDomains[j]));
				
				domainMemory[j].dom = allDomains[j];
				
				unsigned int flags = VIR_DOMAIN_MEM_CURRENT;
        			if(virDomainSetMemoryStatsPeriod(domainMemory[j].dom,n,flags) != -1){
			                printf("Memory collection window set to %d seconds\n",n);
			        }			
	
				domainMemory[j] = returnDomainMemoryStruct(domainMemory[j].dom);
		
				printf("Check returnedValue - %lu\n",domainMemory[j].maxMemory);
			}

			for(j=0;j<numActiveDomains;j++) {
				
				unsigned long long int usedMemory = domainMemory[j].availableMemory - domainMemory[j].unusedMemory;
				long double memoryPercentage = ((long double)usedMemory*100)/(domainMemory[j].availableMemory);
				printf("percent memory usage - %Lf\n", memoryPercentage);	
			
				unsigned long long int memoryToAssign =0;
				if(domainMemory[j].balloonSize < domainMemory[j].maxMemory && memoryPercentage > 90) {
						
				 	memoryToAssign = 1.2*domainMemory[j].balloonSize;
					if(memoryToAssign > domainMemory[j].maxMemory) {
						memoryToAssign = domainMemory[j].maxMemory;
					}
					printf("assigning %llu memory to this domain\n",memoryToAssign);
					virDomainSetMemory(domainMemory[j].dom,memoryToAssign);
				}

				if(domainMemory[j].balloonSize > 0.45*domainMemory[j].maxMemory && memoryPercentage < 30) {
					memoryToAssign = 0.5*domainMemory[j].balloonSize;
                                        printf("assigning %llu memory to this domain\n",memoryToAssign);
                                        virDomainSetMemory(domainMemory[j].dom,memoryToAssign);
				}

				if(domainMemory[j].balloonSize < 0.6*domainMemory[j].maxMemory && memoryPercentage > 55) {
                                        memoryToAssign = 1.1*domainMemory[j].balloonSize;
                                        printf("assigning %llu memory to this domain\n",memoryToAssign);
                                        virDomainSetMemory(domainMemory[j].dom,memoryToAssign);
                                }
				
				if(domainMemory[j].balloonSize < domainMemory[j].maxMemory && memoryPercentage > 70) {
                                        memoryToAssign = 1.1*domainMemory[j].balloonSize;
                                        printf("assigning %llu memory to this domain\n",memoryToAssign);
                                        virDomainSetMemory(domainMemory[j].dom,memoryToAssign);
                                }

			}
			
			time_spent = (double)(clock() - begin)/(double)100;
       			if (time_spent>=(double)n) {
        			printf("\nReset CPU memory allocation\n");


				free(activeDomains);
                		free(allDomains);
                		virConnectClose(conn);
				break;
			}
			free(activeDomains);
			free(allDomains);
			virConnectClose(conn);

		}
	}
return 0;
}


struct DomainMemory returnDomainMemoryStruct(virDomainPtr dom) {

	struct DomainMemory domainMemory;
	
	domainMemory.dom = dom;

	virDomainInfoPtr info;
	info = malloc(sizeof(virDomainInfo));
	virDomainGetInfo(domainMemory.dom,info);	
	printf("max memory for this domain - %lu\n",info->maxMem);
	domainMemory.maxMemory = info->maxMem;
	free(info);
		
	int counter=0;
	struct _virDomainMemoryStat stats[VIR_DOMAIN_MEMORY_STAT_NR];		
	unsigned int nr_stats;
	nr_stats = virDomainMemoryStats (domainMemory.dom, stats, VIR_DOMAIN_MEMORY_STAT_NR, 0);

	while(counter<nr_stats) {
		if(stats[counter].tag == 4) {
			domainMemory.unusedMemory = stats[counter].val;
			printf("unusedMemory - %llu\n",domainMemory.unusedMemory);
		}
		else if (stats[counter].tag == 5) {
			domainMemory.availableMemory = stats[counter].val;
			printf("availableMemory - %llu\n",domainMemory.availableMemory);
		}
		else if (stats[counter].tag == 6) {
		        domainMemory.balloonSize = stats[counter].val;
			printf("balloonSize - %llu\n",domainMemory.balloonSize);
        	}
		else if (stats[counter].tag == 7) {
		        domainMemory.workingSetSize = stats[counter].val;
			printf("workingSetSize - %llu\n",domainMemory.workingSetSize);
        	}
		else if (stats[counter].tag == 8) {
			printf("balloon limit size = %llu\n",stats[counter].val);
		}
        	counter++;
	}
return domainMemory;
}

