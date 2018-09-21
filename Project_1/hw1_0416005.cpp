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
unsigned int time_counter;
unsigned int number_of_players;
typedef struct user_info {
  unsigned int id;
  unsigned int arrive_time;
  unsigned int number_of_times_once;
  unsigned int rest_time;
  unsigned int total_play_rounds;
} USER_INFO;
unsigned int number_of_finished_players;
int global_timer;
int guarantee_number_counter;
vector<string> yes_no_string_list;
// Store each player next arrive time, index is player id
vector<unsigned int> arrive_time;
bool is_playing;
bool current_player_done;
unsigned int player_id;
bool can_detach;

/* Shared with all player threads */
queue<unsigned int> waiting_queue;
sem_t* waiting_queue_write_mutex;

/* Data that transfer to master thread */
typedef struct master_thread_data {
  USER_INFO* user_info_array;
} MASTER_THREAD_DATA;

sem_t* io_mutex;
sem_t* player_start_sem_array;
sem_t* player_done_sem_array;
sem_t* msg_order_sem;
sem_t* all_start_sem;
sem_t* all_complete_sem;
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
  io_mutex = new sem_t;
  player_start_sem_array = new sem_t[number_of_players];
  player_done_sem_array = new sem_t[number_of_players];
  msg_order_sem = new sem_t;
  all_start_sem = new sem_t;
  all_complete_sem = new sem_t;
  sem_init(waiting_queue_write_mutex, 0, 1);
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
  if (io_mutex != NULL) delete io_mutex;
  if (player_start_sem_array != NULL) delete[] player_start_sem_array;
  if (player_done_sem_array != NULL) delete[] player_done_sem_array;
  if (msg_order_sem != NULL) delete msg_order_sem;
  if (all_start_sem != NULL) delete all_start_sem;
  if (all_complete_sem != NULL) delete all_complete_sem;
}

void* master_thread_routine(void* thread_data) {
  MASTER_THREAD_DATA* _thread_data = (MASTER_THREAD_DATA*) thread_data;
  bool is_previous_timeslot_occupied = false;
  global_timer = _thread_data->user_info_array[0].arrive_time;
  number_of_finished_players = 0;
  is_playing = false;
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
    if (is_previous_timeslot_occupied && waiting_queue.empty()) {
      guarantee_number_counter = guarantee_number;
      is_previous_timeslot_occupied = false;
    }
    if (current_player_done) {
      // Remove the the player that just finished his round
      sem_wait(waiting_queue_write_mutex);
        waiting_queue.pop();
      sem_post(waiting_queue_write_mutex);
      is_playing = false;
      is_previous_timeslot_occupied = true;
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
  int rest_rounds_once = user_info->number_of_times_once;
  int rest_rounds = user_info->total_play_rounds;
  bool leave_or_rest = false;
  bool get_prize = false;
  while(true) {
    sem_wait(player_start_sem_array + (user_info->id - 1));
    if(leave_or_rest) {
      sem_wait(io_mutex);
        cout << global_timer << " " << user_info->id << " ";
        cout << FINISH_MESSAGE << yes_no_string_list[get_prize] << endl;
      sem_post(io_mutex);
      leave_or_rest = false;
      sem_post(msg_order_sem);
    }
    else {
      sem_wait(msg_order_sem);
      sem_post(msg_order_sem);
    }
    if(can_detach) break;

    if(!get_prize) {
      if( (int)arrive_time[user_info->id - 1] == global_timer ) {
        // Player should start playing or start waiting line
        sem_wait(waiting_queue_write_mutex);
          waiting_queue.push(user_info->id);
        sem_post(waiting_queue_write_mutex);
        if (waiting_queue.front() == user_info->id) {
          // The player can start playing directly since he's the first who occupy the claw machine
          is_playing = true;
          player_id = user_info->id;
          sem_wait(io_mutex);
            cout << global_timer << " " << user_info->id << " ";
            cout << START_MESSAGE << endl;
          sem_post(io_mutex);
        }
        else {
          /* *****************************************
           * The arrived player can't start playing  *
           * since there's someone ahead of hime in  *
           * the line.                               *
           * This player can only push his ID to the *
           * waiting queue.                          *
           * *****************************************/
          sem_wait(io_mutex);
            cout << global_timer << " " << user_info->id << " ";
            cout << WAIT_MESSAGE << endl;
          sem_post(io_mutex);
        }
      }
      else if (!is_playing 
        && waiting_queue.front() == user_info->id 
        && (int)arrive_time[user_info->id - 1] <= global_timer) {
        // The player that is start playing after line up
        is_playing = true;
        player_id = user_info->id;
        sem_wait(io_mutex);
          cout << global_timer << " " << user_info->id << " ";
          cout << START_MESSAGE << endl;
        sem_post(io_mutex);
      }
      else {}
      // Player now is playing or resting or waiting
      if (is_playing && player_id == user_info->id) {
        --rest_rounds_once;
        --rest_rounds;
        --guarantee_number_counter;
        get_prize = guarantee_number_counter == 0 || (rest_rounds == 0);
        current_player_done = get_prize || rest_rounds_once == 0;
        leave_or_rest = current_player_done;
        guarantee_number_counter = (get_prize) ? guarantee_number : guarantee_number_counter;
        if (current_player_done) {
          number_of_finished_players = (get_prize) ? number_of_finished_players + 1 : number_of_finished_players;
          arrive_time[user_info->id - 1] = (get_prize) ? global_timer : global_timer + 1 + user_info->rest_time ;
          rest_rounds_once = user_info->number_of_times_once;
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

  MASTER_THREAD_DATA* _thread_data = new MASTER_THREAD_DATA;
  thread_array.resize(number_of_players + 1);
  _thread_data->user_info_array = user_info_array;
  arrive_time.resize(number_of_players);
  sort(user_info_array, user_info_array + number_of_players, compare_function_with_arrive_time);
  yes_no_string_list.push_back(" NO");
  yes_no_string_list.push_back(" YES");

  for(unsigned int i = 0; i < number_of_players; ++i) {
    arrive_time[ user_info_array[i].id - 1 ] = user_info_array[i].arrive_time;
  }

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
