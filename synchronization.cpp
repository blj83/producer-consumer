//  synchronization.cpp

#include <iostream>
#include <string>
#include <stdlib.h>
#include <sstream>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fstream>

using namespace std;

int insert = 0;
int consume = 0;
int items_in_buffer = 0;
int bufferCapacity;
bool alive = true;
string *buffer;
pthread_mutex_t PRODUCER_FILE_MUTEX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t CONSUMER_FILE_MUTEX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t BUFFER_MUTEX = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t BUFFER_COUNT = PTHREAD_COND_INITIALIZER;
pthread_key_t glob_var_key;
std::ofstream producerOutputFile;
std::ofstream consumerOutputFile;

string GenerateItem(int pthread_ID, int pthread_Count){
    std::ostringstream temp;
    temp << pthread_ID << "_" << pthread_Count;
    return temp.str();
}
//Method that handles input to be written to file
void WriteToFile(std::ofstream &file, string fileName, string fileInput){
    file.open(fileName.c_str(), std::ofstream::out | std::ofstream::app);
    file << fileInput;
    file.close();
}

void AddItem(string item){
    buffer[insert] = item;
    insert = (insert+1)%bufferCapacity;
    items_in_buffer++;
}


string RemoveItem(){
    string item;
    item = buffer[consume];
    buffer[consume].clear();
    items_in_buffer--;
    consume = (consume + 1) % bufferCapacity;
    return item;
}

void printBuffer(string *buffer, int size){
    for (int i = 0; i<size; i++) {
        cout << buffer[i] << "\n";
    }
}

void *producer(void* id){
    int productionCount = 0;
    int producerID = (int&)id;
    std::ostringstream temp;
    temp << producerID;
    string producerIDString = temp.str();
    srand(time(NULL));
    while(alive){
        bool inserted = false;
	
        string producedString = GenerateItem(producerID,productionCount);
        pthread_mutex_lock(&PRODUCER_FILE_MUTEX);
        WriteToFile(producerOutputFile, "ProducerLog.txt", producerIDString + " Generated "  + producedString +
                    "\n");
        pthread_mutex_unlock(&PRODUCER_FILE_MUTEX);
        do{
            pthread_mutex_lock(&BUFFER_MUTEX);
            if(items_in_buffer==bufferCapacity){ //buffer is full
                pthread_mutex_unlock(&BUFFER_MUTEX);
                pthread_mutex_lock(&PRODUCER_FILE_MUTEX);
                WriteToFile(producerOutputFile, "ProducerLog.txt", producerIDString + " " + producedString + " Buffer Full -- Insertion Failed\n");
                pthread_mutex_unlock(&PRODUCER_FILE_MUTEX);
                sleep((rand() % 2));
            }
            else{//there is room to insert
                AddItem(producedString);
                pthread_mutex_unlock(&BUFFER_MUTEX);
                pthread_mutex_lock(&PRODUCER_FILE_MUTEX);
                WriteToFile(producerOutputFile, "ProducerLog.txt", producerIDString + " " + producedString + " Successful Insertion\n");
                pthread_mutex_unlock(&PRODUCER_FILE_MUTEX);
                inserted = true;
                productionCount+=1;
                pthread_cond_broadcast(&BUFFER_COUNT);
            }
        
        } while(!inserted && alive);
        inserted = false;
        sleep((rand() % 5));
    }
    pthread_exit(NULL);
}
void *consumer(void* id){
	std::ostringstream consumerID;
	consumerID << (int&) id;
	srand(time(NULL));
	bool consumed;
   	while(alive){
		consumed = false;
    		pthread_mutex_lock(&CONSUMER_FILE_MUTEX);
    		WriteToFile(consumerOutputFile, "ConsumerLog.txt", consumerID.str() + " attempting to remove item from buffer\n");
		pthread_mutex_unlock(&CONSUMER_FILE_MUTEX);
		pthread_mutex_lock(&BUFFER_MUTEX);
		do{
		if(items_in_buffer>0){
			//condition variable
			string consumedString = RemoveItem();
			pthread_mutex_unlock(&BUFFER_MUTEX);
			pthread_mutex_lock(&CONSUMER_FILE_MUTEX);
			WriteToFile(consumerOutputFile, "ConsumerLog.txt", consumerID.str() + " " + consumedString + " successfully removed.\n");
			pthread_mutex_unlock(&CONSUMER_FILE_MUTEX);
			consumed = true;
	    	}
		else{
			pthread_cond_wait(&BUFFER_COUNT, &BUFFER_MUTEX);
		
		}
		}while(!consumed && alive);
		
		sleep(rand() % 5);
	    }
    
    pthread_exit(NULL);
}

//Sigint handler will deal with program closing in a desirable fashion
static void sig_int(int){
    alive = false;
}

int main (int argc,char *argv[]) {
    signal(SIGINT, sig_int);
    if(argc!=4){
	printf("incorrect arg length. Correct usage: numThreads numProducers numConsumers\n");
	pthread_exit(NULL);
    }
    bufferCapacity = atoi(argv[1]);
    int numProducers = atoi(argv[2]);
    int numConsumers = atoi(argv[3]);
    int rc;
    
    pthread_key_create(&glob_var_key, NULL); 
    pthread_t producers[numProducers];
    pthread_t consumers[numConsumers];
    
    //initializing the buffer with correct capacity
    buffer = new string[bufferCapacity];
    
    //initializing the producer threads
    for (int i = 0; i < numProducers; i++){
        rc = pthread_create(&producers[i],NULL,producer,(void*)i);
        if (rc){
            printf("ERROR; Producers return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }
    
    //initializing the consumer threads
    for (int i = 0; i < numConsumers; i++){
        rc = pthread_create(&consumers[i],NULL,consumer,NULL);
        if (rc){
            printf("ERROR; Consumers return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }
    while(alive){
    }
    pthread_mutex_destroy(&BUFFER_MUTEX);
    pthread_mutex_destroy(&CONSUMER_FILE_MUTEX);
    pthread_mutex_destroy(&PRODUCER_FILE_MUTEX);
    pthread_cond_destroy(&BUFFER_COUNT);
    pthread_exit(NULL);
}

