#include<stdio.h>
#include<libvirt/libvirt.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<time.h>
#include<stdbool.h>
#include<math.h>

unsigned long long int getCPUtimeInNanos(virDomainPtr dom, int ncpus, int nparams);
unsigned char* getBitsRight(int totalNumberOfCpus, int cpuToPin);
int repeater = 0;	

struct DomainCPU
{
    	virDomainPtr dom;
    	unsigned long long int cpuUsage1;
	unsigned long long int cpuUsage2;
    	long double percentage;
	long double cpuWeightage;
	int cpuId;
	bool tag;
	long double timeDifference;
	int realCpuId;
};

struct CPU {
	int cpu;
	long double weight;
	bool flag;
	int toggle;
	long double currentWeight;
	int countOfVcpusAttached;
};

int totalNumberOfCpus=0;

int main(int argc, char* argv[]) {

	while(1) {

		clock_t begin;
		double time_spent;
		unsigned int i=0;
		begin=clock();
		
		for(i=0;1;i++) {
			int n = atoi(argv[1]);
			int sleeptime = n;
			
			// connecting to qemu hypervisor
			virConnectPtr conn = virConnectOpen("qemu:///system");
			if(conn==NULL) {
        			fprintf(stderr,"Failed to connect to qemu:///system\n");
        			return 1;
			}
			
			int j=0;
			int numActiveDomains=0;
			numActiveDomains = virConnectNumOfDomains(conn);	
//			printf("numactivedomains = %d\n",numActiveDomains);
			
			virDomainPtr* allDomains;
			allDomains = malloc(sizeof(virDomainPtr)*numActiveDomains);

			int* activeDomains;
			activeDomains = malloc(sizeof(int) * numActiveDomains);
			
			virConnectListDomains(conn,activeDomains,numActiveDomains);

			int ncpus=0;
			//int nparams=0;

			struct DomainCPU domainCPU[numActiveDomains];

			int mapLen=1;
                        int maxInfo=4;

                        virVcpuInfoPtr  vvIPtr;
                        vvIPtr = calloc(maxInfo,sizeof(virVcpuInfo));

                        unsigned char* cpuMaps;
                      	cpuMaps = calloc(maxInfo*mapLen,sizeof(unsigned char*));


			for (j = 0 ; j < numActiveDomains ; j++) {
			        allDomains[j] = virDomainLookupByID(conn, activeDomains[j]);
			        printf("\nActive - %d, %s, Id: %d\t", j+1,virDomainGetName(allDomains[j]), virDomainGetID(allDomains[j]));
			        
				ncpus = virDomainGetCPUStats(allDomains[j], NULL, 0, 0, 0, 0); // ncpus
				totalNumberOfCpus=ncpus;

				domainCPU[j].dom = allDomains[j];
				domainCPU[j].tag = 0;
				

                                virDomainGetVcpus(domainCPU[j].dom,vvIPtr,maxInfo,cpuMaps,mapLen);
                                fprintf(stdout,"Real CPU Number - %d\n",vvIPtr->cpu);
                           //     fprintf(stdout, "NEW cpu time 1- %llu\n",vvIPtr->cpuTime);

				domainCPU[j].cpuUsage1 = vvIPtr->cpuTime;
				domainCPU[j].realCpuId = vvIPtr->cpu;

		       	 //	fprintf(stdout, "cputime for domainId %d after query = %llu\n",virDomainGetID(domainCPU[j].dom),domainCPU[j].cpuUsage1);
			}

			
			sleep(sleeptime);
	
			
			long double totalTimeDifference = 0;
	
			for (j = 0 ; j < numActiveDomains ; j++) {
//				printf("\nDomainName - %s ",virDomainGetName(allDomains[j]));	
				for(int k=0;k<numActiveDomains;k++) {
					if(virDomainGetID(domainCPU[j].dom) == virDomainGetID(allDomains[j])) {
						virDomainGetVcpus(domainCPU[j].dom,vvIPtr,maxInfo,cpuMaps,mapLen);
  //              		                fprintf(stdout, "NEW cpu time 2 -  %llu\n",vvIPtr->cpuTime);
	
        		                        domainCPU[j].cpuUsage2 = vvIPtr->cpuTime;

					        unsigned long long int timediff = domainCPU[j].cpuUsage2-domainCPU[j].cpuUsage1;
						long double sleeptimeconverted = (long double)sleeptime*(long double)1000000000;
						domainCPU[j].timeDifference = (long double)timediff/sleeptimeconverted;
				        	totalTimeDifference += domainCPU[j].timeDifference;
			//			fprintf(stdout, "cpu1 - %llu\tcpu2 - %llu sleeptime - %Lf\tTD - %llu\ttimeDifference - %Lf\n",domainCPU[j].cpuUsage1,domainCPU[j].cpuUsage2,sleeptimeconverted,timediff,domainCPU[j].timeDifference);
						break;		
					}
				}

                        }
			
			for(j = 0 ; j < numActiveDomains ; j++) {
				domainCPU[j].percentage = domainCPU[j].timeDifference/totalTimeDifference;
//				printf("Domain - %s PercentageUsage - %Lf\n",virDomainGetName(domainCPU[j].dom),domainCPU[j].percentage);
			}

			
			long double cpuFactor = (long double)1/(long double)totalNumberOfCpus;			
			//printf("Total Number of CPUs = %d\t,cpuFactor = %Lf\n",totalNumberOfCpus,cpuFactor);
						

			struct CPU cpuDetails[totalNumberOfCpus];

			for(int k=0;k<totalNumberOfCpus;k++) {
                        	cpuDetails[k].weight=0; 
				cpuDetails[k].flag=1;
				cpuDetails[k].toggle=1;
				cpuDetails[k].currentWeight=0;
				cpuDetails[k].countOfVcpusAttached=0;
                        }
	

			long double sortedWeights[numActiveDomains];
			for(j=0;j<numActiveDomains;j++) {
				sortedWeights[j] = domainCPU[j].percentage;
			}
			
			long double temp=0;
			for(j=0;j<numActiveDomains;j++) {
				for(int k=j+1;k<=numActiveDomains;k++) {
                                        if(sortedWeights[j] < sortedWeights[k]){
						temp = sortedWeights[j];
						sortedWeights[j] = sortedWeights[k];
						sortedWeights[k] = temp;		
					}
				}
			}

			
			bool flags[totalNumberOfCpus];
			int shouldWeCalculate = 0;	
			for(j=0;j<numActiveDomains;j++) {
				for(int l=0;l<totalNumberOfCpus;l++) {
					if(domainCPU[j].realCpuId == l) {
						cpuDetails[l].currentWeight += domainCPU[j].percentage;  	
						cpuDetails[l].countOfVcpusAttached++;
					}
				}
			}

			for(int l=0;l<totalNumberOfCpus;l++) {
	
				if(cpuDetails[l].currentWeight > cpuFactor) {
					if(cpuDetails[l].countOfVcpusAttached > 1) {
						flags[l] = true;
						shouldWeCalculate = 5;
						break;
					}
					else {
						flags[l] = false;
                                        }
				}
				else {
                                	if(cpuDetails[l].countOfVcpusAttached > 1) {
						flags[l] = false;
                                        }
					else {
						flags[l] = true;
                                        }
                                }
				if(flags[l]) {
					shouldWeCalculate++;
				}
				else {
					shouldWeCalculate--;
				}
			}

			if(shouldWeCalculate >= 0 || repeater > 6) {
			printf("\nCalculating and resetting CPU affinities - This can occur if system pcpu-vcpu distribution is vaguely unequal or as part of a periodic activity(after each 7 cycles)\n");
			
			repeater = 0;
			int count=0;
			for(j=0;j<numActiveDomains;j++) {
				for(int l=0;l<totalNumberOfCpus;l++) {
					if(cpuDetails[l].flag) {
						for(int k=0;k<numActiveDomains;k++) {
							if((domainCPU[k].tag==0) && (domainCPU[k].percentage == sortedWeights[count])) {
								cpuDetails[l].weight+=domainCPU[k].percentage;
	                	                                domainCPU[k].cpuId = l;
        	                //	                        printf("assigning Domain - %s to cpuId %d\n",virDomainGetName(domainCPU[k].dom),domainCPU[k].cpuId);
								domainCPU[k].tag=1;
								count++;
								if(cpuDetails[l].weight>cpuFactor) {
                                                       		cpuDetails[l].flag=0;
                                                      //  	printf("This CPU can't take anymore!!\n");
                	                                	}
                        	                        	break;
							}
						}
					}
				}
                                for(int l=totalNumberOfCpus-1;l>=0;l--) {
                                        if(cpuDetails[l].flag) {
                                                for(int k=0;k<numActiveDomains;k++) {
                                                        if((domainCPU[k].tag==0) && (domainCPU[k].percentage == sortedWeights[count])) {
                                                                cpuDetails[l].weight+=domainCPU[k].percentage;
                                                                domainCPU[k].cpuId = l;
                                  //                              printf("assigning Domain - %s to cpuId %d\n",virDomainGetName(domainCPU[k].dom),domainCPU[k].cpuId);
                                                                domainCPU[k].tag=1;
                                                                count++;
                                                                if(cpuDetails[l].weight>cpuFactor) {
                                                                cpuDetails[l].flag=0;
                                                    //            printf("This CPU can't take anymore!!\n");
                                                                }
                                                                break;
                                                        }
                                                }
                                        }
                                }
			}


			printf("New Domain-CPU Mapping :\n");
			for(j=0;j<numActiveDomains;j++) {
				printf("Domain Name %s to be tagged with cpuId %d Now\n",virDomainGetName(domainCPU[j].dom),domainCPU[j].cpuId);

				int mapLen=1;
				int success=-1;
	                        int maxInfo=4;

        	                virVcpuInfoPtr  vvIPtr;
                	        vvIPtr = calloc(maxInfo,sizeof(virVcpuInfo));

                        	unsigned char* cpuMaps;
                        	cpuMaps = calloc(maxInfo*mapLen,sizeof(unsigned char*));

				virDomainGetVcpus(domainCPU[j].dom,vvIPtr,maxInfo,cpuMaps,mapLen);	
				//fprintf(stdout,"Initial CPU Id - %d\n",vvIPtr->cpu);
				//fprintf(stdout, "NEW cpu time 1- %llu\n",vvIPtr->cpuTime);
					
				//printf("changing the bitmap to pin desired cpuId - %d to this Domain\n",domainCPU[j].cpuId);
			
				if(vvIPtr->cpu != domainCPU[j].cpuId) { 	
				cpuMaps = getBitsRight(totalNumberOfCpus,domainCPU[j].cpuId);				
				
				//fprintf(stdout, "bitmap after change - %d\n", cpuMaps[0]);
				
				
				success = virDomainPinVcpu(domainCPU[j].dom,0,cpuMaps,mapLen);
				
				if(success==-1){
        				printf("\n UNABLE to Map Please Check.\n");
				}
				else {
					printf("pinned to desired cpu\n");
				}
				}
				else {
					printf("domain already pinned to desired CPU not changing the mapping\n");
				}
				
				free(vvIPtr);
				free(cpuMaps);			
				free(allDomains[j]);
			}
			}
			else {
                                printf("\ncpu-vcpu affinities remain unchanged\n");
				repeater++;
                        }	
				
			time_spent = (double)(clock() - begin)/(double)100;
       			if (time_spent>=(double)n) {
        			printf("\nNEXT CYCLE\n");



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

unsigned char* getBitsRight(int totalNumberOfCpus, int cpuToPin) {
	unsigned char* cpuMaps;
	cpuMaps = calloc(totalNumberOfCpus,sizeof(unsigned char*));
	memset(cpuMaps,'0',totalNumberOfCpus);
//	printf("Inside CPU BitChanger");

	cpuMaps[0] = (int)pow(2,cpuToPin);	

return cpuMaps;

} 
