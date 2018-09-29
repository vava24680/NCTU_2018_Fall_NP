#include <iostream>
#include <fstream>
#include <semaphore.h>
#include <pthread.h>
#include <vector>
#include <queue>
#include <string>
#include <algorithm>
#include <unistd.h>

using namespace std;

#define START_MESSAGE "start playing"
#define WAIT_MESSAGE "wait in line"
#define FINISH_MESSAGE "finish playing"

int guarantee_number;
unsigned int number_of_players;
typedef struct user_info {
  unsigned int id;
  unsigned int arrive_time;
  unsigned int number_of_times_once;
  unsigned int rest_time;
  unsigned int total_play_rounds;
} USER_INFO;
// Record how many players have got their prize
unsigned int number_of_finished_players;
// Global time counter
int global_timer;
// Guarantee number countdown counter
int guarantee_number_counter;
// Store "YES" and "NO" strings
vector<string> yes_no_string_list;
// Indicate if the claw machine is playing or not
bool is_claw_machine_playing;
// Indicate is the current player is done or not
bool current_player_done;
// Record the if of the player who is currently playing
unsigned int player_id;
// Flag used by master thread for informing all player threads can detach
bool can_detach;

// Shared with all player threads
queue<unsigned int> waiting_queue;

// Data that transfer to master thread
typedef struct master_thread_data {
  USER_INFO* user_info_array;
} MASTER_THREAD_DATA;

// Semaphore used by each player thread and master thread
// as a lock for shared resource waiting_queue
sem_t* waiting_queue_write_mutex;
// Semaphore used by each player thread as a lock for shared resouce player_id
sem_t* player_id_mutex;
// Semaphore used by each player thread as a lock for shared IO resource
sem_t* io_mutex;
// Semaphores used by master thread for waking up each player thread
sem_t* player_start_sem_array;
// Semaphores used by each player thread for informing the master thread
// that it has done its task in one timeslot
sem_t* player_done_sem_array;
// Semaphore used by each player thread for ensuring the finish messages
// is printed before any start/wait messgaes
sem_t* msg_order_sem;
// Semaphore used by main routine to inform master thread can start its routine
sem_t* all_start_sem;
// Semaphore used by master thread to inform main routine that all players have
// got their prize
sem_t* all_complete_sem;
// vector for storing all thread id
vector<pthread_t> thread_array;

bool compare_function_with_arrive_time(const USER_INFO &first, const USER_INFO &second) {
  return first.arrive_time < second.arrive_time;
}

USER_INFO* file_input(const char* filename) {
    fstream fin;
    fin.open(filename, ios::in);
    if (!fin) {
      cout << "Open file failed" << endl;
      return NULL;
    }
    fin >> guarantee_number;
    fin >> number_of_players;
    USER_INFO* user_info_array = new USER_INFO[number_of_players];
    for(unsigned int i = 0; i < number_of_players; ++i) {
      user_info_array[i].id = i + 1;
      fin >> user_info_array[i].arrive_time;
      fin >> user_info_array[i].number_of_times_once;
      fin >> user_info_array[i].rest_time;
      fin >> user_info_array[i].total_play_rounds;
    }
    fin.close();
    return user_info_array;
}

void init_semaphore() {
  waiting_queue_write_mutex = new sem_t;
  player_id_mutex = new sem_t;
  io_mutex = new sem_t;
  player_start_sem_array = new sem_t[number_of_players];
  player_done_sem_array = new sem_t[number_of_players];
  msg_order_sem = new sem_t;
  all_start_sem = new sem_t;
  all_complete_sem = new sem_t;
  sem_init(waiting_queue_write_mutex, 0, 1);
  sem_init(player_id_mutex, 0, 1);
  sem_init(io_mutex, 0, 1);
  for(unsigned int i = 0; i < number_of_players; ++i) {
    sem_init(player_start_sem_array + i, 0, 0);
    sem_init(player_done_sem_array + i, 0, 0);
  }
  sem_init(msg_order_sem, 0, 0);
  sem_init(all_start_sem, 0, 0);
  sem_init(all_complete_sem, 0, 0);
}

void free_semaphore() {
  if (waiting_queue_write_mutex != NULL) delete waiting_queue_write_mutex;
  if (player_id_mutex != NULL) delete player_id_mutex;
  if (io_mutex != NULL) delete io_mutex;
  if (player_start_sem_array != NULL) delete[] player_start_sem_array;
  if (player_done_sem_array != NULL) delete[] player_done_sem_array;
  if (msg_order_sem != NULL) delete msg_order_sem;
  if (all_start_sem != NULL) delete all_start_sem;
  if (all_complete_sem != NULL) delete all_complete_sem;
}

void* master_thread_routine(void* thread_data) {
  MASTER_THREAD_DATA* _thread_data = (MASTER_THREAD_DATA*) thread_data;

  global_timer = _thread_data->user_info_array[0].arrive_time;
  number_of_finished_players = 0;
  player_id = 0;
  is_claw_machine_playing = false;
  current_player_done = false;
  can_detach = false;
  guarantee_number_counter = guarantee_number;

  sem_wait(all_start_sem);
  sem_post(msg_order_sem);
  while(number_of_finished_players < number_of_players) {
    for(unsigned int i = 0; i < number_of_players; ++i) {
      sem_post(player_start_sem_array + i);
    }
    for(unsigned int i = 0; i < number_of_players; ++i) {
      sem_wait(player_done_sem_array + i);
    }
    sem_wait(msg_order_sem);

    if (!is_claw_machine_playing) {
      guarantee_number_counter = guarantee_number;
    }

    // Check if there's any finished player in this timeslot
    if (current_player_done) {
      // If there is any player on the waiting queue,
      // assign him to the claw machine that is finished by
      // another player in this timeslot, so the waiting player
      // will know that he can play by checking
      // the variable player_id
      if (!waiting_queue.empty()) {
        player_id = waiting_queue.front();
        waiting_queue.pop();
      }
      else {
        player_id = 0;
      }
      is_claw_machine_playing = false;
    }
    else {
      sem_post(msg_order_sem);
    }
    current_player_done = false;
    ++global_timer;
  }
  can_detach = true;
  for(unsigned int i = 0; i < number_of_players; ++i) {
    sem_post(player_start_sem_array + i);
  }
  for(unsigned int i = 0; i < number_of_players; ++i) {
    sem_wait(player_done_sem_array + i);
  }
  sem_post(all_complete_sem);
  pthread_detach(pthread_self());
  return NULL;
}

void* player_thread_routine(void* thread_data) {
  USER_INFO* user_info = (USER_INFO*)thread_data;
  int rest_continuous_rounds = user_info->number_of_times_once;
  int rest_total_rounds = user_info->total_play_rounds;
  // True means the player is either playing or waiting
  // False, otherwise
  bool already_line_up = false;
  /****************************************
   * True means the player has got prize  *
   *    or finished his continuous rounds *
   * False, otherwise                     *
   ****************************************/
  bool leave_or_rest = false;
  bool get_prize = false;
  /****************************************
   * 0-based, indicate which claw machine *
   *    is playing by the player.         *
   * if its value is -1, means the player *
   *    is not playing on a claw machine  *
   ****************************************/
  int claw_machine_id = -1;
  unsigned int arrive_time = user_info->arrive_time;

  while(true) {
    sem_wait(player_start_sem_array + (user_info->id - 1));
    // Check if the player has got a prize
    // or finished his continuous rounds at previous timeslot
    // so he can print the finish messages at first
    if(leave_or_rest) {
      sem_wait(io_mutex);
        cout << global_timer << " " << user_info->id << " ";
        cout << FINISH_MESSAGE << yes_no_string_list[get_prize] << endl;
      sem_post(io_mutex);
      leave_or_rest = false;
      claw_machine_id = -1;
      already_line_up = false;
      sem_post(msg_order_sem);
    }
    // This routine will halt until the finish messages is printed,
    // this make the finish message is printed before any start 
    // and wait messages
    else {
      sem_wait(msg_order_sem);
      sem_post(msg_order_sem);
    }
    if(can_detach) break;

    if(!get_prize) {
      if( (int)arrive_time == global_timer ) {
        // Check if the claw machine is not occupied
        // by checking the player_id is zero or not
        sem_wait(player_id_mutex);
        if (player_id == 0) {
          claw_machine_id = 0;
          player_id = user_info->id;
          is_claw_machine_playing = true;
          already_line_up = true;
          sem_wait(io_mutex);
            cout << global_timer << " " << user_info->id << " ";
            cout << START_MESSAGE << endl;
          sem_post(io_mutex);
        }
        sem_post(player_id_mutex);

        // If the claw machine is occupied, join himself to waiting queue
        if (!already_line_up) {
          sem_wait(waiting_queue_write_mutex);
            waiting_queue.push(user_info->id);
          sem_post(waiting_queue_write_mutex);
          already_line_up = true;
          sem_wait(io_mutex);
            cout << global_timer << " " << user_info->id << " ";
            cout << WAIT_MESSAGE << endl;
          sem_post(io_mutex);
        }
      }
      // The player who is waiting will enter this section
      else if (global_timer > (int)arrive_time && claw_machine_id < 0) {
        // Exhaustive check if the claw machine is not occupied
        sem_wait(player_id_mutex);
        if (player_id == user_info->id) {
          claw_machine_id = 0;
          is_claw_machine_playing = true;
          sem_wait(io_mutex);
            cout << global_timer << " " << user_info->id << " ";
            cout << START_MESSAGE << endl;
          sem_post(io_mutex);
        }
        sem_post(player_id_mutex);
      }
      else {}
      // Player has to do something after one play with the claw machine
      if (claw_machine_id >= 0) {
        --rest_continuous_rounds;
        --rest_total_rounds;
        --guarantee_number_counter;

        get_prize = guarantee_number_counter == 0 || (rest_total_rounds == 0);
        guarantee_number_counter = (get_prize) ? guarantee_number : guarantee_number_counter;
        current_player_done = get_prize || rest_continuous_rounds == 0;
        leave_or_rest = current_player_done;

        // Player has finished his coutinuous round or game
        if (leave_or_rest) {
          number_of_finished_players = (get_prize) ? number_of_finished_players + 1 : number_of_finished_players;
          arrive_time = (get_prize) ? global_timer : global_timer + 1 + user_info->rest_time ;
          rest_continuous_rounds = user_info->number_of_times_once;
        }
      }
    }
    sem_post(player_done_sem_array + (user_info->id - 1));
  }
  sem_post(player_done_sem_array + (user_info->id - 1));
  pthread_detach(pthread_self());
  return NULL;
}

int main(int argc, char* argv[]) {
  USER_INFO* user_info_array;
  user_info_array = file_input(argv[1]);

  if (!user_info_array) return 0;

  MASTER_THREAD_DATA* _thread_data = new MASTER_THREAD_DATA;
  thread_array.resize(number_of_players + 1);
  _thread_data->user_info_array = user_info_array;
  sort(user_info_array, user_info_array + number_of_players, compare_function_with_arrive_time);
  yes_no_string_list.push_back(" NO");
  yes_no_string_list.push_back(" YES");

  init_semaphore();

  pthread_create(&thread_array[0], NULL, &master_thread_routine, (void*)(_thread_data));
  for(unsigned int i = 1; i <= number_of_players; ++i) {
    pthread_create(&thread_array[i], NULL, &player_thread_routine, (void*)(user_info_array + (i - 1)));
  }

  sem_post(all_start_sem);
  sem_wait(all_complete_sem);

  if (user_info_array != NULL) delete[] user_info_array;
  if (_thread_data != NULL) delete _thread_data;
  free_semaphore();
  return 0;
}
