#include <iostream>
#include <fstream>
#include <semaphore.h>
#include <pthread.h>
#include <vector>
#include <queue>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>

using namespace std;

#define START_MESSAGE "start playing"
#define WAIT_MESSAGE "wait in line"
#define FINISH_MESSAGE "finish playing"
#define NUMBER_OF_CLAW_MACHINE 2

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
// Each element with index i indicates weather claw machine is playing or not
vector<bool> claw_machine_is_playing;
// Each element with index i indicates that the player is finish playing with claw machine i
vector<bool> current_player_done;
// Each element with index i indicates claw machine is playing by who
vector<unsigned int> claw_machine_player_id;
// Flag used by master thread for informing all player threads can detach
bool can_detach;

// Global waiting queue for storing id of all waiting players
queue<unsigned int> global_waiting_queue;

// Data structure that master thread needs
typedef struct master_thread_data {
  USER_INFO* user_info_array;
} MASTER_THREAD_DATA;

// Semaphore used by each player thread for as a lock for shared resource global_waiting_queue
sem_t* global_waiting_queue_write_mutex;
// Semaphore used by each player thread for as a lock for shared resource claw_machine_player_id
sem_t* player_id_vector_mutex;
// Semaphore used by each player thread for as a lock for shared resource number_of_finished_players
sem_t* number_of_finished_players_mutex;
// Semaphore used by each player thread for as a lock for shared resource guarantee_number_counter
sem_t* guarantee_number_counter_mutex;
// Semaphore used by each plater thread for as a lock for shares resource current_player_done
sem_t* current_player_done_mutex;
// Semaphore used by each player thread for as a lock for shared IO resource
sem_t* io_mutex;
// Semaphores used by master thread for waking up each player thread
sem_t* player_start_sem_array;
// Semaphores used by each player thread for informing the master thread
// that it has done its task in one timeslot
sem_t* player_done_sem_array;
// Semaphore used by each player thread for ensuring the finish messages
// is printed before any start or wait messages
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
  global_waiting_queue_write_mutex = new sem_t;
  player_id_vector_mutex = new sem_t;
  number_of_finished_players_mutex = new sem_t;
  guarantee_number_counter_mutex = new sem_t;
  current_player_done_mutex = new sem_t;
  io_mutex = new sem_t;
  player_start_sem_array = new sem_t[number_of_players];
  player_done_sem_array = new sem_t[number_of_players];
  msg_order_sem = new sem_t;
  all_start_sem = new sem_t;
  all_complete_sem = new sem_t;
  sem_init(global_waiting_queue_write_mutex, 0, 1);
  sem_init(player_id_vector_mutex, 0, 1);
  sem_init(number_of_finished_players_mutex, 0, 1);
  sem_init(guarantee_number_counter_mutex, 0 , 1);
  sem_init(current_player_done_mutex, 0, 1);
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
  if (global_waiting_queue_write_mutex != NULL) delete global_waiting_queue_write_mutex;
  if (player_id_vector_mutex != NULL) delete player_id_vector_mutex;
  if (number_of_finished_players_mutex != NULL) delete number_of_finished_players_mutex;
  if (guarantee_number_counter_mutex != NULL) delete guarantee_number_counter_mutex;
  if (current_player_done_mutex != NULL) delete current_player_done_mutex;
  if (io_mutex != NULL) delete io_mutex;
  if (player_start_sem_array != NULL) delete[] player_start_sem_array;
  if (player_done_sem_array != NULL) delete[] player_done_sem_array;
  if (msg_order_sem != NULL) delete msg_order_sem;
  if (all_start_sem != NULL) delete all_start_sem;
  if (all_complete_sem != NULL) delete all_complete_sem;
}

void* master_thread_routine(void* thread_data) {
  MASTER_THREAD_DATA* _thread_data = (MASTER_THREAD_DATA*) thread_data;
  bool someone_finished_previous_timeslot = false;

  // global time counter
  global_timer = _thread_data->user_info_array[0].arrive_time;
  // Record how many players have got their prize
  number_of_finished_players = 0;
  claw_machine_is_playing.resize(NUMBER_OF_CLAW_MACHINE, false);
  current_player_done.resize(NUMBER_OF_CLAW_MACHINE, false);
  claw_machine_player_id.resize(NUMBER_OF_CLAW_MACHINE, 0);
  can_detach = false;
  guarantee_number_counter = guarantee_number;

  // Wait until the main routine wake master thread
  sem_wait(all_start_sem);

  while(number_of_finished_players < number_of_players) {
    bool reset_guarantee_number_counter = true;
    // Wake up all player threads;
    for(unsigned int i = 0; i < number_of_players; ++i) {
      sem_post(player_start_sem_array + i);
    }
    // Make sure player threads will pass the routine that
    // is responsible for printing the finish message before
    // other players try to print start or wait message.
    if (!someone_finished_previous_timeslot) {sem_post(msg_order_sem);}

    // Wait until all player threads have done their task in one timeslot
    for(unsigned int i = 0; i < number_of_players; ++i) {
      sem_wait(player_done_sem_array + i);
    }
    // Wait the semaphore msg_order_sem so its value will become 0,
    // so the routine for printing finish message can be executed before
    // players try to lock the claw machine resources
    sem_wait(msg_order_sem);
    // Check if guarantee_number_counter need to reset
    for(unsigned int i = 0; i < NUMBER_OF_CLAW_MACHINE; ++i) {
      reset_guarantee_number_counter &= (!claw_machine_is_playing.at(i));
    }
    // Extra message for debuging
    /*
    if (reset_guarantee_number_counter) {
      cout << global_timer << ", reset_guarantee_number_counter" << endl;
    }
    */
    guarantee_number_counter = (reset_guarantee_number_counter) ? guarantee_number : guarantee_number_counter;
    someone_finished_previous_timeslot = false;
    // Check if there's any finished player in this timeslot
    for(unsigned int i = 0; i < NUMBER_OF_CLAW_MACHINE; ++i) {
      if (current_player_done.at(i)) {
        // If there is any player on the global waiting queue,
        // assign him to the claw machine that is finshed by
        // another player in this timeslot, so the waiting player
        // will know the he can play through player id on the claw machine
        if (!global_waiting_queue.empty()) {
          claw_machine_player_id.at(i) = global_waiting_queue.front();
          global_waiting_queue.pop();
        }
        else {
          claw_machine_player_id.at(i) = 0;
        }
        claw_machine_is_playing.at(i) = false;
      }
      someone_finished_previous_timeslot |= current_player_done.at(i);
      current_player_done.at(i) = false;
    }
    ++global_timer;
  }
  can_detach = true;
  // Use the semaphore to make sure that the last finished player
  // would have enough time to print the finish message
  // before the master thread return
  for(unsigned int i = 0; i < number_of_players; ++i) {
    sem_post(player_start_sem_array + i);
  }
  for(unsigned int i = 0; i < number_of_players; ++i) {
    sem_wait(player_done_sem_array + i);
  }
  // Inform the main routine that all players have got their prize
  sem_post(all_complete_sem);
  pthread_detach(pthread_self());
  return NULL;
}

void* player_thread_routine(void* thread_data) {
  USER_INFO* user_info = (USER_INFO*)thread_data;
  int rest_continuous_rounds = user_info->number_of_times_once;
  int rest_total_rounds = user_info->total_play_rounds;
  // Indicate that if this player is playering/waiting(true) or not(false)
  bool already_line_up = false;
  // Indicate that if this player has got prize
  // or finished it continuous rounds at previous timeslot
  bool leave_or_rest = false;
  // Indicate if the player has got his prize
  // true => got a prize
  // false => not yet
  bool get_prize = false;
  // 0-based id, -1 means the player is not playing
  int claw_machine_id = -1;
  // Record the next time that player will come back
  unsigned int arrive_time = user_info->arrive_time;

  while(true) {
    sem_wait(player_start_sem_array + (user_info->id - 1));
    // Check if the player has got a prize
    // or finished his continuous rouds at previous timeslot
    if(leave_or_rest) {
      sem_wait(io_mutex);
        cout << global_timer << " " << user_info->id << " ";
        cout << FINISH_MESSAGE << yes_no_string_list[get_prize] << " #" << claw_machine_id + 1 << endl;
      sem_post(io_mutex);
      leave_or_rest = false;
      claw_machine_id = -1;
      sem_post(msg_order_sem);
    }
    else {
      // This routine ensures the finish messages is printed
      // before any start/wait messages
      sem_wait(msg_order_sem);
      sem_post(msg_order_sem);
    }

    if(can_detach) break;

    // If he hasn't got a prize
    if(!get_prize) {
      if( (int)arrive_time == global_timer ) {
        // Check if there is any machine that is empty for playing
        sem_wait(player_id_vector_mutex);
        for(unsigned int i = 0; i < NUMBER_OF_CLAW_MACHINE && !already_line_up; ++i) {
          if (claw_machine_player_id.at(i) == 0) {
            claw_machine_id = i;
            claw_machine_player_id.at(i) = user_info->id;
            claw_machine_is_playing.at(i) = true;
            already_line_up = true;
            sem_wait(io_mutex);
              cout << global_timer << " " << user_info->id << " ";
              cout << START_MESSAGE << " #" << claw_machine_id + 1 << endl;
            sem_post(io_mutex);
          }
        }
        sem_post(player_id_vector_mutex);

        // If all claw machines are occupied by others, join himself to global waiting queue
        if (!already_line_up) {
          sem_wait(global_waiting_queue_write_mutex);
            global_waiting_queue.push(user_info->id);
          sem_post(global_waiting_queue_write_mutex);
          already_line_up = true;
          sem_wait(io_mutex);
            cout << global_timer << " " << user_info->id << " ";
            cout << WAIT_MESSAGE << endl;
          sem_post(io_mutex);
        }
      }
      // The player is in waiting queue for playing
      else if (global_timer > (int)arrive_time && claw_machine_id < 0){
        // Exhaustive search for an non-occupied claw machine for playing
        sem_wait(player_id_vector_mutex);
        for(unsigned int i = 0; i < NUMBER_OF_CLAW_MACHINE; ++i) {
          if (claw_machine_player_id.at(i) == user_info->id) {
            claw_machine_id = i;
            claw_machine_is_playing.at(i) = true;
            sem_wait(io_mutex);
              cout << global_timer << " " << user_info->id << " ";
              cout << START_MESSAGE << " #" << claw_machine_id + 1 << endl;
            sem_post(io_mutex);
          }
        }
        sem_post(player_id_vector_mutex);
      }
      else{}
      // Player has to do something druing his playing with claw machine
      if (claw_machine_id >= 0) {
        --rest_continuous_rounds;
        --rest_total_rounds;
        // Lock the resource guarantee_number_counter for decrement 1
        sem_wait(guarantee_number_counter_mutex);
          --guarantee_number_counter;
          get_prize = guarantee_number_counter == 0 || (rest_total_rounds == 0);
          /*
          if (guarantee_number_counter == 0) {
            sem_wait(io_mutex);
            cout << global_timer << ", 保底中獎, id: " << user_info->id << endl;
            sem_post(io_mutex);
          }
          */
          guarantee_number_counter = (get_prize) ? guarantee_number : guarantee_number_counter;
        sem_post(guarantee_number_counter_mutex);

        sem_wait(current_player_done_mutex);
        current_player_done.at(claw_machine_id) = get_prize || rest_continuous_rounds == 0;
        sem_post(current_player_done_mutex);
        leave_or_rest = current_player_done.at(claw_machine_id);
        already_line_up = !current_player_done.at(claw_machine_id);

        // player has finished his round/game
        if (current_player_done.at(claw_machine_id)) {
          // lock the resource number_of_finished_players for increment 1
          sem_wait(number_of_finished_players_mutex);
            number_of_finished_players = (get_prize) ? number_of_finished_players + 1 : number_of_finished_players;
          sem_post(number_of_finished_players_mutex);
          arrive_time = (get_prize) ? global_timer : global_timer + 1 + user_info->rest_time;
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

  MASTER_THREAD_DATA* _thread_data = new MASTER_THREAD_DATA;
  thread_array.resize(number_of_players + 1);
  _thread_data->user_info_array = user_info_array;
  sort(user_info_array, user_info_array + number_of_players, compare_function_with_arrive_time);
  yes_no_string_list.push_back(" NO");
  yes_no_string_list.push_back(" YES");

  srand( time(NULL) );

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
