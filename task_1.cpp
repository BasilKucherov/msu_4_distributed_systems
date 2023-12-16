#include <iostream>
#include <string>
#include <map>
#include <vector>

#include "mpi.h"

#define MPI_NROWS 4
#define MPI_NCOLS 4

class Transputer
{
public:
    enum class Position { CornerBottomLeft, CornerBottomRight, CornerTopLeft, CornerTopRight, EdgeLeft, EdgeRight, EdgeBottom, EdgeTop, Invalid};

    Transputer(int rank, int nrows, int ncols) {
        rank_ = rank;
        nrows_ = nrows;
        ncols_ = ncols;
        row_ = rank_to_row(rank_, ncols_);
        col_ = rank_to_col(rank_, ncols_);
        left_neighbour_ = get_left_neighbour_rank(row_, col_, nrows_, ncols_);
        right_neighbour_ = get_right_neighbour_rank(row_, col_, nrows_, ncols_);
        top_neighbour_ = get_top_neighbour_rank(row_, col_, nrows_, ncols_);
        bottom_neighbour_ = get_bottom_neighbour_rank(row_, col_, nrows_, ncols_);
        position_ = determine_position(row_, col_, nrows_, ncols_);
    }

    int corner_to_corner_double_way(char* message, int message_count) {
        // message goes left edge then top edge
        char* message_up_right = message;
        int message_up_right_count = message_count / 2;

        // message goes bottom edge then right edge
        char* message_right_up = message + message_up_right_count;
        int message_right_up_count = message_count - message_up_right_count;

        // buffer
        char* buff = nullptr;
        int buff_size = 0;

        MPI_Buffer_detach(&buff, &buff_size); 
        delete[] buff;

        int send_message_number = 2;
        MPI_Buffer_attach(malloc(message_count + send_message_number * MPI_BSEND_OVERHEAD), message_count + send_message_number * MPI_BSEND_OVERHEAD);        


        MPI_Status status;
        int mpi_status = 0;

        int do_send_recv = 1;
        int recv_from = 0;
        int send_to = 0;

        switch (position_)
        {
        case Position::CornerBottomLeft:
            // Send up and right (Initial sender)
            mpi_status = MPI_Bsend(message_right_up, message_right_up_count, MPI_CHAR, right_neighbour_, 0, MPI_COMM_WORLD);
            mpi_status = MPI_Bsend(message_up_right, message_up_right_count, MPI_CHAR, top_neighbour_, 0, MPI_COMM_WORLD);

            do_send_recv = 0;
            break;
        case Position::CornerTopRight:
            // Recv left and bottom (Final reciever)
            MPI_Recv(message_right_up, message_right_up_count, MPI_CHAR, bottom_neighbour_, 0, MPI_COMM_WORLD, &status);
            MPI_Recv(message_up_right, message_up_right_count, MPI_CHAR, left_neighbour_, 0, MPI_COMM_WORLD, &status);
            do_send_recv = 0;

            break;
        case Position::CornerBottomRight:
            message = message_right_up;
            message_count = message_right_up_count;

            recv_from = left_neighbour_;
            send_to = top_neighbour_;
            break;
        case Position::CornerTopLeft:
            message = message_up_right;
            message_count = message_up_right_count;

            recv_from = bottom_neighbour_;
            send_to = right_neighbour_;
            break;
        case Position::EdgeBottom:
            message = message_right_up;
            message_count = message_right_up_count;

            recv_from = left_neighbour_;
            send_to = right_neighbour_;
            break;
        case Position::EdgeTop:
            message = message_up_right;
            message_count = message_up_right_count;

            recv_from = left_neighbour_;
            send_to = right_neighbour_;
            break;
        case Position::EdgeLeft:
            message = message_up_right;
            message_count = message_up_right_count;

            recv_from = bottom_neighbour_;
            send_to = top_neighbour_;
            break;
        case Position::EdgeRight:
            message = message_right_up;
            message_count = message_right_up_count;

            recv_from = bottom_neighbour_;
            send_to = top_neighbour_;
            break;
        default:
            std::cout << "[" << rank_ << "] " << "Not participate in transfer\n";
            return 1;
        }

        if (do_send_recv) {
            MPI_Recv(message, message_count, MPI_CHAR, recv_from, 0, MPI_COMM_WORLD, &status);
            mpi_status = MPI_Bsend(message, message_count, MPI_CHAR, send_to, 0, MPI_COMM_WORLD);
        }

        // delete buffer
        MPI_Buffer_detach(&buff, &buff_size); 
        delete[] buff;

        return 0;
    }

    int corner_to_corner_even_procs(char* message, int message_count) {
        // split message into consequtive twelweths
        int messages_numbers = 12;
        char** messages = new char*[messages_numbers];
        int* messages_counts = new int[messages_numbers];

        int per_message_count = message_count / messages_numbers;
        int residue = message_count % messages_numbers;
        char* start = message;
        for(int i = 0; i < messages_numbers; i++) {
            messages[i] = start;
            messages_counts[i] = per_message_count + (residue > 0 ? 1 : 0);
            start += messages_counts[i];

            residue--;
        }

        // buffer
        char* buff = nullptr;
        int buff_size = 0;

        MPI_Buffer_detach(&buff, &buff_size); 
        delete[] buff;

        int send_message_number = 2;
        MPI_Buffer_attach(malloc(message_count + send_message_number * MPI_BSEND_OVERHEAD), message_count + send_message_number * MPI_BSEND_OVERHEAD);        

        MPI_Status status;
        int mpi_status = 0;

        // transfer scheme
        std::map<int, std::pair<std::pair<int, int>, std::pair<int, int>>> message_distribution{
            {0, {{0, 6}, {6, 6}}},

            {4, {{0, 4}, {4, 2}}},
            {1, {{6, 2}, {8, 4}}},

            {8, {{0, 3}, {3, 1}}},
            {5, {{4, 2}, {6, 2}}},
            {2, {{8, 1}, {9, 3}}},

            {12, {{-1, -1}, {0, 3}}},
            {9, {{3, 1}, {4, 2}}},
            {6, {{6, 2}, {8, 1}}},
            {3, {{9, 3}, {-1, -1}}},

            {13, {{-1, -1}, {0, 4}}},
            {10, {{4, 2}, {6, 2}}},
            {7, {{8, 4}, {-1, -1}}},

            {14, {{-1, -1}, {0, 6}}},
            {11, {{6, 6}, {-1, -1}}},
            
            {15, {{-1, -1}, {-1, -1}}}
        };

        MPI_Request bottom_request;
        MPI_Request left_request;
        
        if (bottom_neighbour_ != -1) {
            int from_bottom_message_start = message_distribution[bottom_neighbour_].first.first;
            int from_bottom_message_number = message_distribution[bottom_neighbour_].first.second;
            
            char* from_bottom_message = messages[from_bottom_message_start];
            int from_bottom_message_count = 0;

            for(int i = 0; i < from_bottom_message_number; i++)  {
                from_bottom_message_count += messages_counts[from_bottom_message_start + i];
            }


            MPI_Irecv(from_bottom_message, from_bottom_message_count, MPI_CHAR, bottom_neighbour_, 0, MPI_COMM_WORLD, &bottom_request);
        }

        if (left_neighbour_ != -1) {
            int from_left_message_start = message_distribution[left_neighbour_].second.first;
            int from_left_message_number = message_distribution[left_neighbour_].second.second;
            
            char* from_left_message = messages[from_left_message_start];
            int from_left_message_count = 0;

            for(int i = 0; i < from_left_message_number; i++)  {
                from_left_message_count += messages_counts[from_left_message_start + i];
            }

            MPI_Irecv(from_left_message, from_left_message_count, MPI_CHAR, left_neighbour_, 0, MPI_COMM_WORLD, &left_request);
        }

        if (bottom_neighbour_ != -1) {
            MPI_Wait(&bottom_request, MPI_STATUS_IGNORE);
        }

        if (left_neighbour_ != -1) {
            MPI_Wait(&left_request, MPI_STATUS_IGNORE);
        }

        if (message_distribution[rank_].first.first != -1) {
            int to_top_message_start = message_distribution[rank_].first.first;
            int to_top_message_number = message_distribution[rank_].first.second;
            
            char* to_top_message = messages[to_top_message_start];
            int to_top_message_count = 0;

            for(int i = 0; i < to_top_message_number; i++)  {
                to_top_message_count += messages_counts[to_top_message_start + i];
            }

            mpi_status = MPI_Bsend(to_top_message, to_top_message_count, MPI_CHAR, top_neighbour_, 0, MPI_COMM_WORLD);
        }

        if (message_distribution[rank_].second.first != -1) {
            int to_right_message_start = message_distribution[rank_].second.first;
            int to_right_message_number = message_distribution[rank_].second.second;
            
            char* to_right_message = messages[to_right_message_start];
            int to_right_message_count = 0;

            for(int i = 0; i < to_right_message_number; i++)  {
                to_right_message_count += messages_counts[to_right_message_start + i];
            }

            mpi_status = MPI_Bsend(to_right_message, to_right_message_count, MPI_CHAR, right_neighbour_, 0, MPI_COMM_WORLD);
        }

        return 0;
    }

    static Position determine_position(int row, int col, int nrows, int ncols) {
        if (row == 0 && col == 0) {
            return Position::CornerBottomLeft;
        } else if (row == 0 && col == ncols - 1) {
            return Position::CornerBottomRight;
        } else if (row == nrows - 1 && col == 0) {
            return Position::CornerTopLeft;
        } else if (row == nrows - 1 && col == ncols - 1) {
            return Position::CornerTopRight;
        } else if (col == 0) {
            return Position::EdgeLeft;
        } else if (col == ncols - 1) {
            return Position::EdgeRight;
        } else if (row == 0) {
            return Position::EdgeBottom;
        } else if (row == nrows - 1) {
            return Position::EdgeTop;
        }

        return Position::Invalid;
    }

    static int rank_to_row(int rank, int ncols) {
        return rank / ncols;
    }

    static int rank_to_col(int rank, int ncols) {
        return rank % ncols;
    }

    static int is_valid_cords(int row, int col, int nrows, int ncols) {
        return row >= 0 && row < nrows && col >= 0 && col < ncols;
    }

    static int cords_to_rank(int row, int col, int nrows, int ncols) {
        return is_valid_cords(row, col, nrows, ncols) ?  row * ncols + col : -1;
    }

    static int get_top_neighbour_rank(int row, int col, int nrows, int ncols) {
    return cords_to_rank(row + 1, col, nrows, ncols);
    }

    static int get_bottom_neighbour_rank(int row, int col, int nrows, int ncols) {
        return cords_to_rank(row - 1, col, nrows, ncols);
    }

    static int get_left_neighbour_rank(int row, int col, int nrows, int ncols) {
        return cords_to_rank(row, col - 1, nrows, ncols);
    }

    static int get_right_neighbour_rank(int row, int col, int nrows, int ncols) {
        return cords_to_rank(row, col + 1, nrows, ncols);
    }

private:
    int rank_;
    int nrows_;
    int ncols_;
    int row_;
    int col_;
    int left_neighbour_;
    int right_neighbour_;
    int top_neighbour_;
    int bottom_neighbour_;
    Position position_;
};


void fill_message_alphabet(char* message, int size) {

    for(int i = 0; i < size; i++) {
        message[i] = 'A' + i % ('Z' - 'A' + 1);
    }
    message[size - 1] = '\n';
}

void print_char_arr(char* str, int size) {
    for(int i = 0; i < size; i++) {
        std::cout << str[i];
    }
    std::cout << "\n";
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int message_count = 1e9;
    
    if (argc >= 2) {
        message_count = std::stoi(argv[1]);
    }

    int mpi_rank, mpi_size;
    
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    Transputer transputer(mpi_rank, MPI_NROWS, MPI_NCOLS);

    // Transfer from (0, 0) to (3, 3)
    // Create message
    char* message = new char[message_count];
    if(mpi_rank == 0) {
        fill_message_alphabet(message, message_count);
        std::cout << "Message to send: \n";
        // print_char_arr(message, message_count);
    }

    // split message into right and up  / up and rigt directions
    MPI_Barrier(MPI_COMM_WORLD); 
    double start = MPI_Wtime();

    // transputer.corner_to_corner_double_way(message, message_count);
    transputer.corner_to_corner_even_procs(message, message_count);

    MPI_Barrier(MPI_COMM_WORLD); 
    double end = MPI_Wtime();

    if (mpi_rank == 15) {
        std::cout << "Got message: \n";
        // print_char_arr(message, message_count);
    }

    if (mpi_rank == 0) {
        std::cout << "Transfer time: " << end - start << "s\n";
    }

    delete[] message;

    MPI_Finalize();
    return 0;
}
