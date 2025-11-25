// Online C++ compiler to run C++ program online
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

namespace GBN {

const unsigned int TIMEOUT_MS = (MAX_SEQ+2)*1000;
typedef enum {boolean_false, boolean_true} boolean;

typedef unsigned int seq_nr;
typedef struct {unsigned char data[MAX_PKT];} packet;
typedef enum {data, ack, nak} frame_kind;

typedef struct {
    frame_kind kind;
    seq_nr seq;
    seq_nr ack;
    packet info;
} frame;

typedef enum {frame_arrival, cksum_err, timeout, network_layer_ready} event_type;
event_type event;

struct Timer {
    bool active = false;
    steady_clock::time_point start_time;

    void start() {
        active = true;
        start_time = steady_clock::now();
    }

    void stop() { active = false; }

    bool expired() {
        if (!active) return false;
        auto now = steady_clock::now();
        return duration_cast<milliseconds>(now - start_time).count() > TIMEOUT_MS;
    }
};

deque<frame> transmission_wire;
bool network_ready = false;
vector<Timer> timers(MAX_SEQ + 1);

void wait_for_event(event_type *event) {
    for (int i = 0; i <= MAX_SEQ; i++) {
        if (timers[i].active && timers[i].expired()) {
            cout<<"Frame - "<<i<<"is timed out"<<endl;
            *event = timeout;
            return;
        }
    }
    static int event_counter = 0;
    event_type events[] = {network_layer_ready, frame_arrival};
    *event = events[event_counter % 3];
    event_counter++;
    if(*event==frame_arrival)
    {
        int r = rand() % 100;
        if(r<20)
        {
            *event=cksum_err;
        }
    }

    if (network_ready) {
        *event = network_layer_ready;
        event_counter=0;
    }
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




///**noise version **************************
void to_physical_layer(frame *s) {
int r = rand() % 100;

if (r < 15) {
    cout << "Physical layer: Frame LOST/ERROR - seq=" << s->seq << endl;
    return;
}

// Otherwise frame goes into transmission wire
transmission_wire.push_back(*s);

cout << "Physical layer: Sent Frame - seq=" << s->seq
     << " ack=" << s->ack
     << " data=" << s->info.data << endl;

}

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



static boolean between(seq_nr a, seq_nr b, seq_nr c) {
    if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
        return(boolean_true);
    else
        return(boolean_false);
}

static void send_data(seq_nr frame_nr, seq_nr frame_expected, packet buffer[]) {
    frame s;
    s.kind = data;
    s.info = buffer[frame_nr];
    s.seq = frame_nr;
    s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);
    to_physical_layer(&s);
    start_timer(frame_nr);
}

void protocol5() {
    cout << "=== PROTOCOL 5 (Go-Back-N) STARTED ===" << endl;
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

                if (r.seq == frame_expected) {
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
                    cout << "Receiver: Discarding out-of-order frame " << r.seq
                         << " (expected: " << frame_expected << ")" << endl;
                }

                break;

            case cksum_err:
                from_physical_layer(&r);
                nbuffered = nbuffered - 1;
                cout << "Event: Checksum error - ignoring frame"<<r.seq << endl;
                break;

            case timeout:
                cout << "Event: Timeout - Go Back N retransmission" << endl;
                next_frame_to_send = ack_expected;
                transmission_wire.clear();
                for (i = 0; i < nbuffered; i++) {
                    cout << "Retransmitting frame " << next_frame_to_send << endl;
                    send_data(next_frame_to_send, frame_expected, buffer);
                    inc(next_frame_to_send);
                }
                break;
        }

        cout << "State: ack_expected=" << ack_expected<<endl
             << "next_frame_to_send=" << next_frame_to_send<<endl
             << "frame_expected=" << frame_expected<<endl
             << "nbuffered=" << nbuffered << endl;
        cout << "----------------------------------------" << endl;
if(!(event==frame_arrival||event==cksum_err ||(event==network_layer_ready && network_ready==false)))
std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

} // namespace GBN

namespace GBN_SR {

const unsigned int TIMEOUT_MS = (MAX_SEQ+1)*1000;
typedef enum {boolean_false, boolean_true} boolean;

typedef unsigned int seq_nr;
typedef struct {
	unsigned char dataa[MAX_PKT];
} packet;
typedef enum {dataa, ack, nak} frame_kind;
// global to indicate which sequence timed out (invalid value means none)
int timed_out_seq = -1;


typedef struct {
	frame_kind kind;
	seq_nr seq;
	seq_nr ack;
	packet info;
} frame;

typedef enum {frame_arrival, cksum_err, timeout, network_layer_ready} event_type;
event_type event;


void printDeque(const deque<frame>& q) {
	cout << "Queue elements: ";
	for (const auto &f : q) {
		cout << f.seq << " "; // print the sequence number for clarity
	}
	cout << endl;
}
void printrDeque(const deque<frame>& q) {
	cout << "Reciver Queue elements: ";
	for (const auto &f : q) {
		cout << f.seq << " "; // print the sequence number for clarity
	}
	cout << endl;
}


struct Timer {
	bool active = false;
	steady_clock::time_point start_time;
	void start() {
		active = true;
		start_time = steady_clock::now();
	}

	void stop() {
		active = false;
	}

	bool expired() {
		if (!active) return false;
		auto now = steady_clock::now();
		return duration_cast<milliseconds>(now - start_time).count() > TIMEOUT_MS;
	}
	long long time() {
		if (!active) return false;
		auto now = steady_clock::now();
		return  duration_cast<milliseconds>(now - start_time).count() ;
	}
};
deque <frame> reciverQueue;
deque<frame> transmission_wire;
bool network_ready = false;
vector<Timer> timers(MAX_SEQ + 1);
bool retransmition =false;

void wait_for_event(event_type *event) {
	// reset timed_out_seq
	timed_out_seq = -1;

	for (int i = 0; i <= MAX_SEQ; i++) {
		/// debug line to see the clk
		cout<<"Timer seq-"<<i<<"="<<timers[i].time()<<endl ;
		if (timers[i].active && timers[i].expired()) {
			cout << "Frame - " << i << " is timed out" << endl;
			*event = timeout;
			timed_out_seq = i;   // record which frame timed out
			return;
		}



	}
	static int event_counter = 0;
	event_type events[] = {network_layer_ready, frame_arrival};
	*event = events[event_counter % 2];
	event_counter++;
	if(*event==frame_arrival)
	{
		int r = rand() % 100;
		if(r<5&&retransmition==false)
		{
			*event=cksum_err;
		}
	}



	if (network_ready) {
		*event = network_layer_ready;
		event_counter=0;
	}
}

void from_network_layer(packet *p, seq_nr seq) {
	string dataa_str = "Packet-" + to_string(seq);
	strncpy((char*)p->dataa, dataa_str.c_str(), MAX_PKT-1);
	p->dataa[MAX_PKT-1] = '\0';
	cout << "Network layer: Generated packet for seq=" << seq << endl;
}

void to_network_layer(packet *p) {
	cout << "Network layer: Received packet: " << p->dataa << endl;
}


void from_physical_layer(frame *r) {
	if (transmission_wire.empty()) return;

	*r = transmission_wire.front();
	transmission_wire.pop_front();

	if(event!=cksum_err)
		r->ack = (r->seq + 1) % (MAX_SEQ + 1);
}




///**noise version **************************
void to_physical_layer(frame *s) {
	int r = rand() % 100;

	if (r < 18&&retransmition==false) {
		cout << "Physical layer: Frame LOST/ERROR - seq=" << s->seq << endl;
		return;
	}

// Otherwise frame goes into transmission wire
	transmission_wire.push_back(*s);

	cout << "Physical layer: Sent Frame - seq=" << s->seq
	     << " ack=" << s->ack
	     << " dataa=" << s->info.dataa << endl;
	retransmition=false;

}

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



static boolean between(seq_nr a, seq_nr b, seq_nr c) {
	if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
		return(boolean_true);
	else
		return(boolean_false);
}

static void send_dataa(seq_nr frame_nr, seq_nr frame_expected, packet buffer[]) {
	frame s;
	s.kind = dataa;
	s.info = buffer[frame_nr];
	s.seq = frame_nr;
	s.ack = (frame_expected ) % (MAX_SEQ + 1);
	to_physical_layer(&s);
	start_timer(frame_nr);
}

void protocol5() {
	cout << "=== PROTOCOL 5 (Go-Back-N) STARTED ===" << endl;
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
			if(network_ready) {
				cout << "Event: Network layer ready" << endl;
				from_network_layer(&buffer[next_frame_to_send], next_frame_to_send);
				nbuffered = nbuffered + 1;
				send_dataa(next_frame_to_send, frame_expected, buffer);
				inc(next_frame_to_send);
			}
			else
				cout<<"Event: Network layer is ready but Data link halt transmission "<<endl;
			break;
		case frame_arrival:
			cout << "Event: Frame arrival" << endl;
			from_physical_layer(&r);
				//simulating sending ack for teh recived frame with no check sum
				stop_timer(r.seq);
				nbuffered--;
				cout << "Sender: ACK for frame " << r.seq << " processed" << endl;
			if (r.seq == frame_expected) {
				frame buff;
				to_network_layer(&r.info);
				inc(frame_expected);
				//debuging buffer check

				while(reciverQueue.size()&&(reciverQueue.front().seq==frame_expected))
				{
					frame buff ;
					buff=reciverQueue.front();
					reciverQueue.pop_front();
					cout << "Receiver: pulling frame in order from queue: ; "<< buff.seq << endl;
					inc(frame_expected);
					cout <<"Frame expected"<<frame_expected<<endl	;
					to_network_layer(&buff.info);

				}



			}

			else
			{
				cout << "Receiver: Out of order frame recived and buffering " << r.seq << endl;
				reciverQueue.push_back(r);

			}


			break;

		case cksum_err:
			from_physical_layer(&r);
			cout << "Event: Checksum error - ignoring frame"<<r.seq << endl;
			break;

		case timeout:
			cout << "Event: Timeout - retransmit timed frame only" << endl;
			if (timed_out_seq >= 0 && timed_out_seq <= MAX_SEQ) {
				// retransmit only the frame whose timer expired
				cout << "Retransmitting single timed-out frame " << timed_out_seq << endl;
				// restart its timer (send_dataa already starts timer)

				// send_dataa expects frame_nr and piggyback ack = frame_expected
				retransmition=true;
				send_dataa((seq_nr)timed_out_seq, frame_expected, buffer);

			} else {
				// fallback: if for some reason timed_out_seq not set, retransmit from ack_expected
				cout << "Timed out seq unknown falling back to retransmit from ack_expected" << endl;

				transmission_wire.clear();
				for (i = 0; i < nbuffered; i++) {
					cout << "Retransmitting frame " << next_frame_to_send << endl;
					send_dataa(ack_expected, frame_expected, buffer);

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
			cout<<"clockcycle counted"<<endl;
		}
		printDeque(transmission_wire);

		printrDeque(reciverQueue);
		}
}

} // namespace GBN_SR

int main() {
    srand(time(0));

    cout << "Select protocol:" << endl;
    cout << "1 - Go-Back-N" << endl;
    cout << "2 - GBN-SR (Selective Repeat)" << endl;
    cout << "Enter choice: ";

    int choice;
    cin >> choice;

    if (choice == 1) {
        GBN::protocol5();
    } else if (choice == 2) {
        GBN_SR::protocol5();
    } else {
        cout << "Invalid choice" << endl;
    }

    return 0;
}
