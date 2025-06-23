#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <semaphore.h>
 
struct Student{
	int id;
	int help;
};
 
struct PriorityQueueNode{
  struct Student *s;
	struct PriorityQueueNode *next;
};
 
struct PriorityQueue{
	struct PriorityQueueNode *head;
  struct PriorityQueueNode *tail;
};
 
struct WaitingQueue{
	struct Student **student;
  int head;
  int tail;
  int size;
  int capacity;
};
 
 
//Global variables
int student_count;
int tutor_count;
int chair_count;
int helps;
int total_requests = 0;
int total_sessions = 0;
int students_being_tutored = 0;
int occupied_chairs = 0;
int *tutor;
struct WaitingQueue *waitingQueue;
struct PriorityQueue *priorityQueue;
 
sem_t coordinator_sem;
sem_t *student_sems;
sem_t tutor_sem;
pthread_mutex_t q_mutex;
pthread_mutex_t counter_mutex;
pthread_mutex_t chairs_mutex;
 
 
//Function declaration
void* coordinator_thread(void *arg);
void* student_thread(void *arg);
void* tutor_thread(void *arg);
int is_wq_full(struct WaitingQueue *q);
int is_wq_empty(struct WaitingQueue *q);
void enqueue_wq(struct WaitingQueue *q, struct Student *s);
struct Student* dequeue_wq(struct WaitingQueue *q);
struct Student* peekqueue_wq(struct WaitingQueue *q);
void enqueue_pq(struct PriorityQueue *pq, struct Student *s);
struct Student* dequeue_pq(struct PriorityQueue *pq);
 
 
//Program start
int main(int argc, char *argv[]){
  assert(argc == 5 && "Not correct number of arguments");
 
	student_count = atoi(argv[1]);
	tutor_count = atoi(argv[2]);
	chair_count = atoi(argv[3]);
	helps = atoi(argv[4]);
  assert(student_count > 0 && tutor_count > 0 && "Invalid number of students or tutors");
 
  //waiting queue init
  waitingQueue = malloc(sizeof(struct WaitingQueue));
  waitingQueue->student = malloc(chair_count * sizeof(struct Student*));
  waitingQueue->head = -1;
  waitingQueue->tail = -1;
  waitingQueue->size = 0;
  waitingQueue->capacity = chair_count;
  
  //priority queue init
  priorityQueue = malloc(helps * sizeof(struct PriorityQueue));
  assert(priorityQueue != NULL && "Memory allocation failed for PriorityQueue array");
  int a=0;
  for(a=0;a<helps;a++){
    priorityQueue[a].head = NULL;
    priorityQueue[a].tail = NULL;
  }
  
  //tutor id array
  tutor = malloc(student_count * sizeof(int));
  assert(tutor != NULL && "Memory allocation failed for tutor array");
  for(a=0;a<student_count;a++){
    tutor[a] = -1;
  }
  
  //initialize semaphores
  assert(sem_init(&coordinator_sem, 0, 0) == 0 && "Error in coordinator_sem init");
 
  assert(pthread_mutex_init(&q_mutex, NULL) == 0 && "Error in q_mutex init");
 
  assert(pthread_mutex_init(&counter_mutex, NULL) == 0 && "Error in counter_mutex init");
  
  assert(pthread_mutex_init(&chairs_mutex, NULL) == 0 && "Error in chairs_mutex init");
 
  assert(sem_init(&tutor_sem, 0, 0) == 0 && "Error in tutor_sem init");
  
  student_sems = malloc(student_count * sizeof(sem_t));
  for(a=0;a<student_count;a++){
    assert(sem_init(&student_sems[a], 0, 0) == 0 && "Error in student_sems init");
  }
  
	//coordinator thread creation
	pthread_t coordinator;
  assert(pthread_create(&coordinator, NULL, coordinator_thread, "C") == 0 && "Error in coordinator thread creation");
	//student threads creation
	pthread_t students[student_count];
	//struct student s;
	int i;
	for( i=0;i<student_count;i++){
		struct Student *s = malloc(sizeof(struct Student));
		s->id = i;
		s->help = 0;
    assert(pthread_create(&students[i], NULL, student_thread, s) == 0 && "Error in student thread creation");
	}
 
	//tutor threads creation
	pthread_t tutors[tutor_count];
	int j;
	for(j=0;j<tutor_count;j++){
		int* tutor_id = malloc(sizeof(int));
		*tutor_id = j;
    assert(pthread_create(&tutors[j], NULL, tutor_thread, tutor_id) == 0 && "Error in tutor thread creation");
	}
 
 
	//Wait for all students to complete
	int k;
	for(k=0;k<student_count;k++){
		pthread_join(students[k], NULL);
	}
 
	//clean-up process
  pthread_cancel(coordinator);
  for(k=0;k<tutor_count;k++){
    pthread_cancel(tutors[k]);
  }
  
  sem_destroy(&coordinator_sem);
  sem_destroy(&tutor_sem);
  for(k=0;k<student_count;k++){
    sem_destroy(&student_sems[k]);
  }
  
  free(student_sems); 
  free(waitingQueue->student); 
  free(waitingQueue); 
  free(priorityQueue);
	return 0;
}
 
int is_wq_full(struct WaitingQueue *q){
  return q->size == q->capacity;
}
 
int is_wq_empty(struct WaitingQueue *q){
  return q->size == 0;
}
 
void enqueue_wq(struct WaitingQueue *q, struct Student *s){
  if(is_wq_full(q)){
    return;
  }
  
  if(q->head == -1){
    q->head = 0;
  }
  
  q->tail = (q->tail + 1) % q->capacity;
  q->student[q->tail] = s;
  q->size++;
}
 
struct Student* dequeue_wq(struct WaitingQueue *q){
  if(is_wq_empty(q)){
    return NULL;
  } 
  
  struct Student* s = q->student[q->head];
  if(q->head == q->tail){
    q->head = -1;
    q->tail = -1;
  }
  else{
    q->head = (q->head + 1) % q->capacity;
  }
  
  q->size--;
  return s;
}
 
void enqueue_pq(struct PriorityQueue *pq, struct Student *s){
  int priority = s->help;
  struct PriorityQueueNode *newNode = malloc(sizeof(struct PriorityQueueNode));
  newNode->s = s;
  newNode->next = NULL;
  
  // queue is empty
  if(pq[priority].head == NULL){
     pq[priority].head = newNode;
     pq[priority].tail = newNode;
  }
  else{
     pq[priority].tail->next = newNode;
     pq[priority].tail = newNode;
  }
}
 
struct Student* dequeue_pq(struct PriorityQueue *pq){
  int i;
  for(i=0; i<helps;i++){
     if(pq[i].head != NULL){
       struct PriorityQueueNode *node = pq[i].head;
       struct Student *s = node->s;
       pq[i].head = node->next;
       
       if(pq[i].head == NULL){
         pq[i].tail = NULL;
       }
       
       free(node);
       return s;
     }
  }
  return NULL;
}
 
void* coordinator_thread(void* arg){
  
  while(1){
    sem_wait(&coordinator_sem);
    
    pthread_mutex_lock(&q_mutex);
    struct Student *student = dequeue_wq(waitingQueue);
    
    if(student!=NULL){
      enqueue_pq(priorityQueue, student);
      
      printf("C: Student %d with priority %d added to the queue. Waiting students now = %d. Total requests = %d\n",
      student->id,
      helps - student->help - 1,
      occupied_chairs,
      ++total_requests);
      
      sem_post(&tutor_sem);
    }
    
    pthread_mutex_unlock(&q_mutex);
  }
  return NULL;
}
 
void* student_thread(void* arg){
  
  struct Student *student = (struct Student*)arg;
  
  while(student->help < helps){
    usleep(rand() % 2000);
    
    pthread_mutex_lock(&chairs_mutex);
    
    assert(occupied_chairs <= chair_count &&"Occupied chairs cannot exceed total chairs");
    
    if(occupied_chairs < chair_count){
      
      occupied_chairs++;
      pthread_mutex_unlock(&chairs_mutex);
      
      pthread_mutex_lock(&q_mutex);
      //if waiting queue is not full
      enqueue_wq(waitingQueue, student);
 
      printf("S: Student %d takes a seat. Empty chairs = %d\n",student->id, chair_count - occupied_chairs);
      pthread_mutex_unlock(&q_mutex);
      
      sem_post(&coordinator_sem);
      
      sem_wait(&student_sems[student->id]);
      
      //once student has received help
      student->help++;
      
      printf("S: Student %d received help from tutor %d\n", student->id, tutor[student->id]);
      tutor[student->id] = -1;
      
    }
    else{
      //if waiting queue is full (no empty chairs)
      pthread_mutex_unlock(&chairs_mutex);
      printf("S: Student %d found no empty chair. Will try again later.\n",student->id);
    }    
  }
  free(student);
  return NULL;
}
 
 
void* tutor_thread(void* arg){
  
  int tutor_id = *(int*)arg;
  free(arg);
  
  while(1){
    sem_wait(&tutor_sem);
    pthread_mutex_lock(&q_mutex);
    
    struct Student *student = dequeue_pq(priorityQueue);
    
    if(student != NULL){
      
      
      pthread_mutex_unlock(&q_mutex);
      
      /*if(tutor[student->id] == -1){
        tutor[student->id] = tutor_id;
      }
      else{
        printf("Attempting to change an occupied student");
      }*/
      assert((tutor[student->id] == -1 || (printf("Attempting to change an occupied student\n"), 1)) && "Student already has a tutor");
      tutor[student->id] = tutor_id;
      
      sem_post(&student_sems[student->id]);
    
      pthread_mutex_lock(&chairs_mutex);
      occupied_chairs--;
      pthread_mutex_unlock(&chairs_mutex);
      
      
      pthread_mutex_lock(&counter_mutex);
      students_being_tutored++;
      total_sessions++;
      pthread_mutex_unlock(&counter_mutex);
      
      printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d\n",  
      student->id,
      tutor_id, 
      students_being_tutored, 
      total_sessions);
      
      
      usleep(200);
      
      pthread_mutex_lock(&counter_mutex);
      students_being_tutored--;
      pthread_mutex_unlock(&counter_mutex);
      
    }
    else{
      pthread_mutex_unlock(&q_mutex);
    }
  }
  return NULL;
}
 
