#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <cstring>
#include <deque>
#include <cstdlib>
#include <ctime>
#include <thread>

using namespace std;
using namespace std::chrono;

#define inc(k) if (k < MAX_SEQ) k = k + 1; else k = 0
#define MAX_PKT 1024
#define MAX_SEQ 7


const int PROTOCOL_GBN = 1;
const int PROTOCOL_SR = 2;
int selected_protocol = 0;


typedef enum {boolean_false, boolean_true} boolean;
typedef unsigned int seq_nr;

typedef struct {
    unsigned char data[MAX_PKT];
} packet;

typedef enum {frame_data, ack, nak} frame_kind;

typedef struct {
    frame_kind kind;
    seq_nr seq;
    seq_nr ack;
    packet info;
} frame;

typedef enum {frame_arrival, cksum_err, timeout, network_layer_ready} event_type;

event_type event;
deque<frame> transmission_wire;
bool network_ready = false;


int timed_out_seq = -1;
deque<frame> reciverQueue;
bool retransmition = false;

struct Timer {
    bool active = false;
    steady_clock::time_point start_time;

    void start() {
        active = true;
        start_time = steady_clock::now();
    }

    void stop() { active = false; }

    // Modified to accept specific timeout duration
    bool expired(unsigned int timeout_ms) {
        if (!active) return false;
        auto now = steady_clock::now();
        return duration_cast<milliseconds>(now - start_time).count() > timeout_ms;
    }

    long long time() {
        if (!active) return 0;
        auto now = steady_clock::now();
        return duration_cast<milliseconds>(now - start_time).count();
    }
};

vector<Timer> timers(MAX_SEQ + 1);



void start_timer(seq_nr k) {
    if (k <= MAX_SEQ) {
        timers[k].start();
        cout << "Timer: Started for frame " << k << endl;
    }
}

void stop_timer(seq_nr k) {
    if (k <= MAX_SEQ) {
        timers[k].stop();
        cout << "Timer: Stopped for frame " << k << endl;
    }
}

void enable_network_layer(void) {
    network_ready = true;
    cout << "Network layer: Enabled" << endl;
}

void disable_network_layer(void) {
    network_ready = false;
    cout << "Network layer: Disabled" << endl;
}

void from_network_layer(packet *p, seq_nr seq) {
    string data_str = "Packet-" + to_string(seq);
    strncpy((char*)p->data, data_str.c_str(), MAX_PKT-1);
    p->data[MAX_PKT-1] = '\0';
    cout << "Network layer: Generated packet for seq=" << seq << endl;
}

void to_network_layer(packet *p) {
    cout << "Network layer: Received packet: " << p->data << endl;
}

void from_physical_layer(frame *r) {
    if (transmission_wire.empty()) return;

    *r = transmission_wire.front();
    transmission_wire.pop_front();

    if(event!=cksum_err)
        r->ack = (r->seq + 1) % (MAX_SEQ + 1);
}

static boolean between(seq_nr a, seq_nr b, seq_nr c) {
    if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
        return(boolean_true);
    else
        return(boolean_false);
}

void printDeque(const deque<frame>& q) {
    cout << "Queue elements: ";
    for (const auto &f : q) {
        cout << f.seq << " "; 
    }
    cout << endl;
}

void printrDeque(const deque<frame>& q) {
    cout << "Reciver Queue elements: ";
    for (const auto &f : q) {
        cout << f.seq << " ";
    }
    cout << endl;
}



void wait_for_event(event_type *event) {
    int current_timeout;
    if (selected_protocol == PROTOCOL_GBN)
        current_timeout = (MAX_SEQ + 2) * 1000;
    else
        current_timeout = (MAX_SEQ + 1) * 1000;

    
    timed_out_seq = -1;

    for (int i = 0; i <= MAX_SEQ; i++) {
        if (selected_protocol == PROTOCOL_SR) {
             cout<<"Timer seq-"<<i<<"="<<timers[i].time()<<endl ;
        }
        
        if (timers[i].active && timers[i].expired(current_timeout)) {
            if (selected_protocol == PROTOCOL_GBN)
                cout<<"Frame - "<<i<<"is timed out"<<endl;
            else
                cout << "Frame - " << i << " is timed out" << endl;
                
            *event = timeout;
            if (selected_protocol == PROTOCOL_SR) {
                timed_out_seq = i;
            }
            return;
        }
    }

    static int event_counter = 0;
    event_type events[] = {network_layer_ready, frame_arrival};
    
    if (selected_protocol == PROTOCOL_GBN)
        *event = events[event_counter % 3]; 
    else
        *event = events[event_counter % 2];
        
    event_counter++;
    
    if(*event==frame_arrival)
    {
        int r = rand() % 100;
        int threshold = (selected_protocol == PROTOCOL_GBN) ? 20 : 1;
        if(r < threshold)
        {
            *event=cksum_err;
        }
    }

    if (network_ready) {
        *event = network_layer_ready;
        event_counter=0;
    }
}

void to_physical_layer(frame *s) {
    int r = rand() % 100;
    
    bool lost = false;
    if (selected_protocol == PROTOCOL_GBN) {
        if (r < 15) lost = true;
    } else {
        if (r < 18 && retransmition == false) lost = true;
    }

    if (lost) {
        cout << "Physical layer: Frame LOST/ERROR - seq=" << s->seq << endl;
        return;
    }

    // Otherwise frame goes into transmission wire
    transmission_wire.push_back(*s);

    cout << "Physical layer: Sent Frame - seq=" << s->seq
         << " ack=" << s->ack
         << " data=" << s->info.data << endl;
         
    if (selected_protocol == PROTOCOL_SR) {
        retransmition=false;
    }
}

static void send_data(seq_nr frame_nr, seq_nr frame_expected, packet buffer[]) {
    frame s;
    s.kind = frame_data;
    s.info = buffer[frame_nr];
    s.seq = frame_nr;
    
    if (selected_protocol == PROTOCOL_GBN)
        s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);
    else
        s.ack = (frame_expected) % (MAX_SEQ + 1);
        
    to_physical_layer(&s);
    start_timer(frame_nr);
}

void protocol5() {
    if (selected_protocol == PROTOCOL_SR) {
    cout << "=== PROTOCOL 5 selectice repeat STARTED ===" << endl;
    
    }
    else{
    cout << "=== PROTOCOL 5 (Go-Back-N) STARTED ===" << endl;
    
    }   
    seq_nr next_frame_to_send;
    seq_nr ack_expected;
    seq_nr frame_expected;
    frame r;
    packet buffer[MAX_SEQ + 1];
    seq_nr nbuffered;
    seq_nr i;

    enable_network_layer();
    ack_expected = 0;
    next_frame_to_send = 0;
    frame_expected = 0;
    nbuffered = 0;

    while (true) {

        cout<<endl;

        if (nbuffered < MAX_SEQ)
            enable_network_layer();
        else
            disable_network_layer();
            
        wait_for_event(&event);

        switch(event) {
            case network_layer_ready:
                if(network_ready){
                    cout << "Event: Network layer ready" << endl;
                    from_network_layer(&buffer[next_frame_to_send], next_frame_to_send);
                    nbuffered = nbuffered + 1;
                    send_data(next_frame_to_send, frame_expected, buffer);
                    inc(next_frame_to_send);
                }
                else
                    cout<<"Event: Network layer is ready but Data link halt transmission "<<endl;
                break;
                
            case frame_arrival:
                cout << "Event: Frame arrival" << endl;
                from_physical_layer(&r);
                
                if (selected_protocol == PROTOCOL_SR) {
                     stop_timer(r.seq);
                     nbuffered--;
                     cout << "Sender: ACK for frame " << r.seq << " processed" << endl;
                }

                if (r.seq == frame_expected) {
                    if (selected_protocol == PROTOCOL_GBN) {
                        cout << "Receiver: Accepting in-order frame " << r.seq << endl;
                        to_network_layer(&r.info);
                        while (between(ack_expected, (r.ack-1), next_frame_to_send) == boolean_true) {
                            cout << "Sender: Processing ACK for frame " << ack_expected << endl;
                            nbuffered = nbuffered - 1;
                            stop_timer(ack_expected);
                            inc(ack_expected);
                        }
                        inc(frame_expected);
                    } else {
                        
                        frame buff;
                        to_network_layer(&r.info);
                        inc(frame_expected);
                        
                        while(reciverQueue.size()&&(reciverQueue.front().seq==frame_expected))
                        {
                            frame buff ;
                            buff=reciverQueue.front();
                            reciverQueue.pop_front();
                            cout << "Receiver: pulling frame in order from queue: ; "<< buff.seq << endl;
                            inc(frame_expected);
                            cout <<"Frame expected"<<frame_expected<<endl;
                            to_network_layer(&buff.info);
                        }
                    }
                } else {
                    if (selected_protocol == PROTOCOL_GBN) {
                        cout << "Receiver: Discarding out-of-order frame " << r.seq
                             << " (expected: " << frame_expected << ")" << endl;
                    } else {
                         cout << "Receiver: Out of order frame recived and buffering " << r.seq << endl;
                         reciverQueue.push_back(r);
                    }
                }
                break;

            case cksum_err:
                if (selected_protocol == PROTOCOL_GBN) {
                    from_physical_layer(&r);
                    nbuffered = nbuffered - 1;
                    cout << "Event: Checksum error - ignoring frame"<<r.seq << endl;
                } else {
                    from_physical_layer(&r);
                    cout << "Event: Checksum error - ignoring frame"<<r.seq << endl;
                }
                break;

            case timeout:
                if (selected_protocol == PROTOCOL_GBN) {
                    cout << "Event: Timeout - Go Back N retransmission" << endl;
                    next_frame_to_send = ack_expected;
                    transmission_wire.clear();
                    for (i = 0; i < nbuffered; i++) {
                        cout << "Retransmitting frame " << next_frame_to_send << endl;
                        send_data(next_frame_to_send, frame_expected, buffer);
                        inc(next_frame_to_send);
                    }
                } else {
                    cout << "Event: Timeout - retransmit timed frame only" << endl;
                    if (timed_out_seq >= 0 && timed_out_seq <= MAX_SEQ) {
                         cout << "Retransmitting single timed-out frame " << timed_out_seq << endl;
                         retransmition=true;
                         send_data((seq_nr)timed_out_seq, frame_expected, buffer);
                    } else {
                         cout << "Timed out seq unknown falling back to retransmit from ack_expected" << endl;
                         transmission_wire.clear();
                         for (i = 0; i < nbuffered; i++) {
                             cout << "Retransmitting frame " << next_frame_to_send << endl;
                             send_data(ack_expected, frame_expected, buffer);
                         }
                    }
                }
                break;
        }

        cout << "State: ack_expected=" << ack_expected<<endl
             << "next_frame_to_send=" << next_frame_to_send<<endl
             << "frame_expected=" << frame_expected<<endl
             << "nbuffered=" << nbuffered << endl;
        cout << "----------------------------------------" << endl;
        
        if(!(event==frame_arrival||event==cksum_err ||(event==network_layer_ready && network_ready==false))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (selected_protocol == PROTOCOL_SR) cout<<"clockcycle counted"<<endl;
        }
        
        if (selected_protocol == PROTOCOL_SR) {
             printDeque(transmission_wire);
             printrDeque(reciverQueue);
        }
    }
}

int main() {
    srand(time(0));

    cout << "Select protocol:" << endl;
    cout << "1 - Go-Back-N" << endl;
    cout << "2 - Selective Repeat" << endl;
    cout << "Enter choice: ";

    int choice;
    cin >> choice;

    if (choice == 1) {
        selected_protocol = PROTOCOL_GBN;
        protocol5();
    } else if (choice == 2) {
        selected_protocol = PROTOCOL_SR;
        protocol5();
    } else {
        cout << "Invalid choice" << endl;
    }

    return 0;
}
