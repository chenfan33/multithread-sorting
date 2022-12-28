#include <stdio.h>
#include<string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <vector> 
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>

using namespace std;

int PROCESS_NUM = 4;
#define P_NUM 4;

void bubbleSort(long arr[], long n);
long* readFiles(int argc, char * argv[],int optind, int &count);
int min(int a, int b);
vector<long> mergeArrays(vector<vector<long>> data, int count);
void forkProcess(int count, long* data);
void *threadSort(void *vargp);
void createThread(int count, long*data);

typedef struct thread_info {
    int tid;
    int length;
    int parent_pipe[2];
    int child_pipe[2];
} th;

int main(int argc, char *argv[])
{
  if (argc == 1){
    fprintf (stderr, "Chen Fan, cfan3\n");
    return (0);   
  }

  int tflag = 0;
  int nflag = 0;
  int c;

  while ((c = getopt (argc, argv, "tn:")) != -1){
    switch (c){
      case 't':
        tflag = 1;
        break;
      case 'n':
        nflag = 1;
        if (isdigit(*optarg)){
          PROCESS_NUM = atoi(optarg);
        }
        else{
          optind --;
        }

        break;
      case '?':
        if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c',please include -n inputFile!!\n", optopt);
        else
          fprintf (stderr,
                   "Unknown option character `\\x%x',please include -n inputFile!!\n",
                   optopt);
      return 1;
      default:
        abort ();
      }
  }

  if (optind == argc){
    fprintf (stderr, "No input file!\n");
    return (0);
  }

  if (nflag == 0){
    fprintf (stderr, "Invalid input, please include -n inputFile!\n");
    return (0);   
  }
  // record the total number of data
  int count;
  long * data = readFiles(argc, argv, optind, count);

  if (not tflag){
    forkProcess(count, data);
  }
  else{
    createThread(count, data);
  }
	return (0);
}

/**
 * Thread function that read from parent pipe, bubble sort,
 * and write to child pipe
*/
void *threadSort(void *vargp){
  thread_info *info = (thread_info *) vargp;

  int length = info->length;
  long stored[length];

  //read from parent pipes here
  for(int i = 0; i < length; i++){
    read(info->parent_pipe[0], &stored[i], sizeof(stored[i]));
  }      
  //close parent read
  close(info->parent_pipe[0]);

  bubbleSort(stored, length);

  for(int i = 0; i < length; i++){
    write(info->child_pipe[1], &stored[i], sizeof(stored[i]));
  }   
  //close child write
  close(info->child_pipe[1]);
  return NULL;
}

/**
 * Given long* data, and its length, create n threads
*/
void createThread(int count, long*data){
  // store sorted vectors from subprocesses
  vector<vector<long> > childSorted(PROCESS_NUM);
  int chunk_size = (int) ceil((double) count/PROCESS_NUM);
  pthread_t threads[PROCESS_NUM];
  thread_info tds[PROCESS_NUM];
  for(int j = 0; j < PROCESS_NUM; j++){
    int length = min(chunk_size, count-j*chunk_size);

    //create child thread and store pipes, tid, length
    thread_info *child;
    child = (thread_info *) malloc(sizeof(struct thread_info));
    if (pipe(child->parent_pipe) == -1){
      fprintf (stderr, "pipe() failed\n");
      exit(0); 
    }
    if (pipe(child->child_pipe) == -1){
      fprintf (stderr, "pipe() failed\n");
      exit(0); 
    }
    child->length = length;
    child->tid = threads[j];
    tds[j] = *child;
    
    if (pthread_create(&threads[j], NULL, threadSort, (void *)child) != 0){
      fprintf (stderr, "pthread_create() failed\n");
      exit(0); 
    }
  } 

  for (int j = 0; j < PROCESS_NUM; j++){
    int length = min(chunk_size, count-j*chunk_size);

    //start writing to parent write end
    for(int i = 0; i < length; i++){
      write(tds[j].parent_pipe[1], &data[j*chunk_size+i], sizeof(data[i]));
    }
 
    //close child read end
    close(tds[j].parent_pipe[1]); 
  }

  for (int j = 0; j < PROCESS_NUM; j++){
    int length = min(chunk_size, count-j*chunk_size);

    //reading from child
    long res[length];
    childSorted[j].resize(length);
    for(int i = 0; i < length; i++){
      read(tds[j].child_pipe[0], &res[i], sizeof(res[i]));      
      childSorted[j][i] = res[i];
    }    

    //close child read end
    close(tds[j].child_pipe[0]); 
  }

  vector<long> merged = mergeArrays(childSorted, count);
  for (int i = 0; i < merged.size();i++){
    printf("%ld\n", merged[i]);
  }

  for (int j=0; j < PROCESS_NUM; j++){
    pthread_join(threads[j],NULL);    
  }
  pthread_exit(NULL);
  return;
}

/**
 * count: total number of data, long*data: pointer to the data
 * fork n processes
*/
void forkProcess(int count, long* data){
    // used to store result from sorted subprocess
    vector<vector<long> > childSorted(PROCESS_NUM);

    int chunk_size = (int) ceil((double) count/PROCESS_NUM);
    int pipes[2*PROCESS_NUM][2];
    pid_t pid[PROCESS_NUM];

    for(int j = 0; j < PROCESS_NUM; j++){
      // create pipe
      if (pipe(pipes[j*2]) == -1) {
        fprintf (stderr, "Failed to create pipe!\n");
        exit(0); 
      }
      if (pipe(pipes[j*2+1]) == -1) {
        fprintf (stderr, "Failed to create pipe!\n");
        exit(0); 
      }
      pid[j] = fork();
      if (pid[j] < 0){
        fprintf (stderr, "Failed to create subprocess!\n");
        exit(0); 
      }

      // if in the child process
      else if (pid[j] == 0){

        //close parent write side
        close(pipes[j*2][1]);

        //close child read
        close(pipes[j*2+1][0]);

        int length = min(chunk_size, count-j*chunk_size);
        //read from parent pipes[j*2][0] here
        long stored[length];
        for(int i = 0; i < length; i++){
          read(pipes[j*2][0], &stored[i], sizeof(stored[i]));
        }      

        //close parent read
        close(pipes[j*2][0]);

        bubbleSort(stored, length);

        for(int i = 0; i < sizeof(stored); i++){
          write(pipes[j*2+1][1], &stored[i], sizeof(stored[i]));
        }      
        // close child write
        close(pipes[j*2+1][1]);
        exit (0); 
      }
      else{
        // close parent read end
        close(pipes[j*2][0]);

        //close child write end
        close(pipes[j*2+1][1]);

        //start writing to parent write end
        int length = min(chunk_size, count-j*chunk_size);
        for(int i = 0; i < length; i++){
          write(pipes[j*2][1], &data[j*chunk_size+i], sizeof(data[i]));
        }

        //close parent write end
        close(pipes[j*2][1]);

      }
    }
    
    // read from children and exit
    for (int j =0; j < PROCESS_NUM; j ++){
        //reading from child and store in res
        int length = min(chunk_size, count-j*chunk_size);
        long res[length];
        childSorted[j].resize(length);
        for(int i = 0; i < length; i++){
          read(pipes[j*2+1][0], &res[i], sizeof(res[i]));
          childSorted[j][i] = res[i];
        }    
        //close child read
        close(pipes[j*2+1][0]);
    }

    for (int j = 0; j < PROCESS_NUM; j++){
        if ( waitpid(pid[j], NULL, 0) == -1 ) {
          fprintf (stderr, "waitpid failed!\n");
          exit(0); 
        }  
    }

    vector<long> merged = mergeArrays(childSorted, count);
    for (int i = 0; i < merged.size();i++){
      printf("%ld\n", merged[i]);
    }
    return;
  }

/**
 * Given vector of vector of long and its length, return a merged sorted array
*/
vector<long> mergeArrays(vector<vector<long>> data, int count){

  vector<long> merged(count);
  for (int i = 0; i < count; i++){
    // store the min value and its index
    long minimum = (long) INFINITY;
    int min_index;
    for (int j = 0; j < PROCESS_NUM; j++){
      if (data[j].size() > 0){
        if (data[j][0] < minimum){
          minimum = data[j][0];
          min_index = j;
        }
      }
    }
    // insert it into merged, and remove it from data
    merged[i] = minimum;
    vector<long>::iterator k = data[min_index].begin();
    data[min_index].erase(k);

  }
  return merged;
}

/**
 * Read the rest of argv as files and return an array storing all data
 * and modify the count
*/
long* readFiles(int argc, char * argv[], int optind,  int &count){

  FILE *file;
  char line[80] = {1};
  int index;
  long * info = NULL;
  int counter = 0;
  for (index = optind; index < argc; index++){
    file = fopen(argv[index], "r");
    if (file == NULL){
      printf("Error opening file %s!\n",argv[index]);    
      exit(0);      
    }

    while(fgets(line, 80, file)!= NULL){
      info = (long *) realloc(info,(counter+1) * sizeof(long *));
      info[counter] = strtol(line, NULL, 10);
      counter++;
    }

    fclose(file);
  }
  count = counter;
  return info;
}

int min(int a, int b){
  return (a > b) ? b:a;
}

/** 
 * Given the int array and its length n
 * sort the array inplace
*/
void bubbleSort(long arr[], long n){
  int i = 0;
  int j = 0;
  long temp;
  for (i = 0; i < n; i++){
    for (j = 0; j < n-1; j++){
        if (arr[j] > arr[j + 1]){
          temp = arr[j];
          arr[j] = arr[j+1];
          arr[j+1] = temp;
        }
    }
  }

}



