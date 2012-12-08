#ifndef BESTMOVE_H
#define BESTMOVE_H

#include "Board.h"

// BestMove accepts ownership of the Move given it via its main constructor.
// Since it has ownership of a dynamic object, it requires a copy
// constructor, operator=, and destructor.
struct BestMove {
   Board::Move *move; // The best move to take (owned by the BestMove)
   long value;            // The board value that will result from the move
   long depth;            // Levels of minimax that were used to get move
   long numBoards;        // Number of boards explored to get move

   BestMove() : move(NULL), value(0), depth(0), numBoards(0) {}
   BestMove(Board::Move *mv, long val, int dpt, long brds)
    : move(mv), value(val), depth(dpt), numBoards(brds) {}
   
   BestMove(const BestMove &mv);
   const BestMove &operator=(const BestMove &mv);
   
   ~BestMove();
   
   void SetBestMove(Board::Move *mv) {delete move; move = mv;}
};

#endif