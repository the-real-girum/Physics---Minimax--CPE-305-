#include <fstream>
#include <assert.h>
#include "SimpleAIPlayer.h"
#include "Book.h"

using namespace std;

// Preconditions: 
// bestMove points to a BestMove object, which may have NULL for its current 
// move.  Any 'bestMove->value' V such that V <= min or V >= max is 
// "uninteresting" to the caller.  
// 
// 'minimaxLevel' is always >= 1 and indicates how deeply to probe. (Note that 
// this means you should evaluate leaf boards directly as they are generated, 
// rather than making a FindBestMove call with minimaxLevel of 0 for each leaf 
// board.  This is important for efficient running.)
// 
// 'tTable' points to a tTable of previously computed BestMoves, or NULL.
//
// 
// Postconditions: 
// 'bestMove->value' indicates the result of the minimax search. If 
// bestMove->value <= min or bestMove->value >= max then no "interesting" move 
// could be found, and bestMove->move will indicate the first move where this 
// fact was uncovered.
// 
// If 'board' is an endgame board, then  bestMove->value will be winVal, 0 or 
// -winVal, and bestMove->move will be NULL since there is no move from an 
// endgame position.  Otherwise, bestMove->move will provide the best move.  
//
// 'bestMove->bestReply' gives the best answering move to bestMove->move, or 
// NULL if bestMove->move ends the game.  
// 
// 'bestMove->numBoards' gives the number of boards examined during the minimax 
// computation, including the root board. 
// 
// *board is left in the same state as it was when the call commenced.  
// 
// 'bestMove->value' gives the minimax computed value for *board.  
// 
// *tTable will contain new entries for any board configurations that were 
// computed during the call, with depth greater than or equal to SAVE_LEVEL, 
// and will have updated any entries for which deeper results were computed 
// during the call.

void SimpleAIPlayer::Minimax(Board *board, int minimaxLevel, long min, long max,
 BestMove *bMove, Book *tTable, int dbg) {
   list<Board::Move *> moves;
   list<Board::Move *>::iterator mIter;
   BestMove subBestMove(NULL, NULL, 0, minimaxLevel, 1);
   const Board::Key *key = 0;
   Book::iterator bIter;
   pair<Book::iterator, bool> ins;

   // [Staley] Level 0 computations aren�t worth it since a call of GetValue is 
   // [Staley] usually quicker than a tTable lookup.
   // [Me] So, ensure that MakeBook doesn't call this method with 
   // minimaxLevel == 0.
   assert(minimaxLevel >= 1);

   // Before we begin "exploring" this node, first consult the transposition 
   // table to see if we already have a precomputed best move for its
   // board configuration [Filled blank] "with minimaxLevel at least as deep 
   // as the one you need."
   if (tTable && (bIter = tTable->find(key = board->GetKey())) != tTable->end()
    && (*bIter).second.depth >= minimaxLevel) {
      // [Filled blank] If we find the bestMove in the transposition table,
      // then set the bestMove straightaway.
      *bMove = (*bIter).second;
      bMove->numBoards = 1;
   }
   else {
      // To begin "exploring" this node, first figure out what the list of
      // possible moves is, so that you can construct the nodes at the 
      // minimaxLevel below you (one node created per Move).
      board->GetAllMoves(&moves);

      // Fill up bestMove -- assume that the bestMove for this node is an empty 
      // BestMove.
      *bMove = subBestMove;

      // Edge case: If this node is an end-game node, then set this bestMove's 
      // value to be the appropriate kWinVal.
      // [Filled blank] Otherwise, bestMove->value should just be the current
      // value of the board.
      bMove->value = moves.size() == 0 ? board->GetValue() :
       (board->GetWhoseMove() ? Board::kWinVal - 1 : -Board::kWinVal + 1);

      // Iterate through each of the possible moves, [Filled blank] provided
      // that the limits for this node haven't collided yet.
      for (mIter = moves.begin(); min < max && mIter != moves.end(); mIter++) {

         board->ApplyMove(*mIter);

         // Base case.  If the minimax recursion can't possibly go down another
         // level because you're at your target Level, then stop recursing down.
         if (minimaxLevel == 1)
            subBestMove.value = board->GetValue();
         else
            Minimax(board, minimaxLevel-1, min, max, &subBestMove, tTable, dbg);

         // [Filled blank] Conditional: White pulls the floor up.
         if (board->GetWhoseMove() == 1 && subBestMove.value > min) {
            bMove->value = min = subBestMove.value;
            
            // [Filled blank] Set the best move to be this move.
            bMove->SetBestMove((*mIter)->Clone());

            // [Filled blank] Set the reply move to be the subBestMove,
            // and nil out subBestMove's move.
            bMove->SetReplyMove(subBestMove.move);
            subBestMove.move = NULL;
         }
         // [Filled blank] Conditional: Black pushes the ceiling down.
         else if (board->GetWhoseMove() == 0 && subBestMove.value < max) {
            bMove->value = max = subBestMove.value;

            // [Filled blank] Set the reply move to be the subBestMove
            bMove->SetBestMove((*mIter)->Clone());

            // [Filled blank] Set the reply move to be the subBestMove,
            // and nil out subBestMove's move.
            bMove->SetReplyMove(subBestMove.move);
            subBestMove.move = NULL;
         }

         if (dbg > 0) {
            for (int cnt = minimaxLevel-1; cnt > 0; cnt--)
               cout << "   ";
            cout << "Move " << (string)**mIter << " nets " << subBestMove.value
             << " min/max is " << min << "/" << max << endl;
         }

         board->UndoLastMove();
         bMove->numBoards += subBestMove.numBoards;
      }

      // [Filled blank] Delete Move pointers contained in any unused nodes.
      for (; mIter != moves.end(); mIter++)
         delete *mIter;

      // [Filled blank] From the loop before, a min/max limits collision doesn't
      // return, but breaks instead (to provide time to clean up the 
      // GetAllMoves() call.  Thus, you have to ensure that the tTable isn't
      // added if you had a min/max collision.
      if (tTable && minimaxLevel >= SAVE_LEVEL && min < max && bMove->move) {
         // [Filled blank] Insert the key->bestMove mapping into the map.
         ins = tTable->insert(pair<const Board::Key *, BestMove>(key, *bMove));
         
         // If you successfully inserted the key, then nil out the pointer to
         // it before you accidentally delete it after this else{} finishes.
         if (ins.second)
            key = 0;
         // [Filled blank] "And, very importantly, we update the table 
         // even if it already has a key for the board you�re computing, if 
         // your new computation is for a deeper lookahead minimaxLevel than 
         // the one in the tTable."
         else if ((*ins.first).second.depth < minimaxLevel) {
            (*ins.first).second = *bMove;
         } 
      }
   }
   delete key;
}


