#include <assert.h>
#include <vector>
#include <memory.h>
#include <cmath>
#include "CheckersDlg.h"
#include "CheckersView.h"
#include "CheckersMove.h"
#include "CheckersBoard.h"
#include "MyLib.h"
#include "BasicKey.h"

using namespace std;


// TODO: Ian had this great idea where you testRun for a large value
// that runs in the background, diffing against Clint's version as you go.
// Since you don't have to adhere to a 0.1 second limit, this can catch
// a lot of SegFault type errors and the sort.

/************************************************************************/
/* Declare/initialize static member datum here                          */
/************************************************************************/
CheckersBoard::Rules CheckersBoard::mRules;
CheckersBoard::Cell CheckersBoard::mCells[kNumCells];
ulong CheckersBoard::mBlackBackSet;
ulong CheckersBoard::mWhiteBackSet;

BoardClass CheckersBoard::mClass("CheckersBoard",
                                 &CreateCheckersBoard,
                                 "Checkers",
                                 &CheckersView::mClass,
                                 &CheckersDlg::mClass,
                                 &CheckersBoard::SetOptions,
                                 &CheckersBoard::GetOptions);
// The C++ definition here isn't required in C++11, which I'm using.
// Put it there anyways to force the "static block" to run.
CheckersBoard::CheckersBoardInitializer CheckersBoard::mInitializer;


void CheckersBoard::StaticInit() {
   Cell *cell;
   char row = 'A';
   unsigned col = 1;
   int nextCell = 0;

   // Initialize mCells, mBlackBackRow and mWhiteBackRow
   for (row = 'A'; row <= 'H'; row ++) {
      for (col = ((row-'A')%2) + 1; col <= kWidth; col += 2, nextCell++) {
      
         // Initialize cell->mask
         cell = mCells + nextCell;
         cell->mask = 1 << nextCell;

         cell->loc = CheckersMove::Location(row, col);

         // Initialize mBlackBackRow
         if (row == 'A') {
            mBlackBackSet |= cell->mask;
         }
         // Initialize mWhiteBackRow
         else if (row == 'H') {
            mWhiteBackSet |= cell->mask;
         } 
      }
   }

   // Go back and initialize the directional pointers.  (Can't initialize
   // in the first pass, since not all cells have been created then).
   nextCell = 0;
   for (row = 'A'; row <= 'H'; row ++) {
      for (col = ((row-'A')%2) + 1; col <= kWidth; col += 2, nextCell++) {

         cell = mCells + nextCell;

         // Set up the directional pointers for mCells.  GetCell() 
         // automatically returns NULL for any out of bounds values.
         cell->neighborCells[kSW] = GetCell(row-1, col-1);
         cell->neighborCells[kSE] = GetCell(row-1, col+1);
         cell->neighborCells[kNW] = GetCell(row+1, col-1);
         cell->neighborCells[kNE] = GetCell(row+1,col+1);
      }
   }
}

CheckersBoard::CheckersBoard() : mWhoseMove(kBlack), 
 mBlackCount(kStartingPieces), mBlackKingCount(0), 
 mBlackBackCount(kStartingBackPieces), mWhiteCount(kStartingPieces), 
 mWhiteKingCount(0), mWhiteBackCount(kStartingBackPieces) {
   // Just to make sure that I'm covering all my bases with ALL member datum
   assert(mMoveHist.size() == 0);

   char row = 'A';
   unsigned col = 1;

   // Nil out the bitmasks at first
   mBlackSet = mWhiteSet = mKingSet = 0x0;

   // Fill up mBlackSet and mWhiteSet
   for (row = 'A'; row <= 'H'; row++) {
      for (col = ((row-'A')%2) + 1; col <= kWidth; col += 2) {

         // Fill up initial mBlackSet
         if (row == 'A' || row == 'B' || row == 'C') {
            mBlackSet |= GetCell(row, col)->mask;
         }
         // Fill up initial mWhiteSet
         else if (row == 'F' || row == 'G' || row == 'H') {
            mWhiteSet |= GetCell(row, col)->mask;
         }
      }
   }
}

CheckersBoard::~CheckersBoard() {
   // Clear out mMoveHistory
   list<Move *>::iterator itr;
   for (itr = mMoveHist.begin(); itr != mMoveHist.end(); itr++)
      delete *itr;
   mMoveHist.clear();
}

long CheckersBoard::GetValue() const {
   // TODO: Handle the case where the game is over due to noone being able
   // to move.
   
   // Black is positive, white is negative
   if (mWhiteCount == 0)
      return kWinVal;
   else if (mBlackCount == 0)
      return -kWinVal;
   else {
      return pieceWgt * (mBlackCount - mWhiteCount) + 
       mRules.kingWgt * (mBlackKingCount - mWhiteKingCount) +
       mRules.backRowWgt * (mBlackBackCount - mWhiteBackCount) +
       mRules.moveWgt * mWhoseMove;
   }
}

void CheckersBoard::ApplyMove(Move *move) {
   CheckersMove *castedMove = dynamic_cast<CheckersMove *>(move);
   CheckersMove::LocVector *locs = &castedMove->mLocs;
   assert(castedMove != NULL && castedMove->mLocs.size() >= 2);
   Set allPieces(mBlackSet | mWhiteSet);
   Cell *originCell = GetCell((*locs)[0]);
   int jumpedPieces = 0; // The number of pieces that move "jumps"
   Piece *pieceToMove = NULL;

   // Assertions on each destination piece.
   for (unsigned int index = 1; index < (*locs).size(); ++index) {
      Cell *cell = GetCell((*locs)[index].first, (*locs)[index].second);
      
      // Assert that this cell (where this move wants to go to) isn't already
      // taken.
      assert((allPieces & cell->mask) == 0);
      
      // Assert that non-king pieces aren't moving backwards.
      if ((originCell->mask & mKingSet) == 0) {
         // Black non-kings should be moving upwards
         if (mWhoseMove == kBlack) {
            assert((*locs)[index].first > (*locs)[index-1].first);
         }
         // White non-kings should be moving downwards
         else if (mWhoseMove == kWhite) {
            assert((*locs)[index].first < (*locs)[index-1].first);
         }
      }
   }

   // Remove the piece from its original location.
   pieceToMove = Take(originCell, mWhoseMove);

   // If this move is a "jump move", then you need to do perform some extra
   // logic to remove cells that you've jumped over.
   if (castedMove->mIsJumpMove) {
      for (unsigned int i = 1; i < (*locs).size(); ++i) {
         Cell *jumpedOverCell = NULL;
         Cell *toCell = GetCell((*locs)[i]);

         // Assert that this location actually jumps over a piece
         assert(CheckersMove::IsJump((*locs)[i-1], (*locs)[i]));

         // If you at any point a non-king jumps into the back row, then assert 
         // that it STOPPED MOVING.
         if (!pieceToMove->isKing) {
            if (mWhoseMove == kBlack && (toCell->mask & mWhiteBackSet))
               assert(i == ((*locs).size() - 1));
            else if (mWhoseMove == kWhite && (toCell->mask & mBlackBackSet))
               assert(i == ((*locs).size() - 1));
         }

         // Figure out which cell you jumped over.
         // TODO: Refactor this.
         // North-west jump
         if ((*locs)[i].first > (*locs)[i-1].first
          && (*locs)[i].second < (*locs)[i-1].second) {
            jumpedOverCell = GetCell((*locs)[i].first - 1, (*locs)[i].second + 1);
         }
         // North-east jump
         else if ((*locs)[i].first > (*locs)[i-1].first
          && (*locs)[i].second > (*locs)[i-1].second) {
            jumpedOverCell = GetCell((*locs)[i].first - 1, (*locs)[i].second - 1);
         }
         // South-west jump
         else if ((*locs)[i].first < (*locs)[i-1].first
          && (*locs)[i].second < (*locs)[i-1].second) {
            jumpedOverCell = GetCell((*locs)[i].first + 1, (*locs)[i].second + 1);
         } 
         // South-east jump
         else if ((*locs)[i].first < (*locs)[i-1].first
          && (*locs)[i].second > (*locs)[i-1].second) {
            jumpedOverCell = GetCell((*locs)[i].first + 1, (*locs)[i].second - 1);
         } else assert(false); 


         // Assert that the piece that you jumped over is occupied by the other
         // player and not yourself.
         if (mWhoseMove == kBlack) {
            assert((jumpedOverCell->mask & mWhiteSet) != 0);
         } else if (mWhoseMove == kWhite) {
            assert((jumpedOverCell->mask & mBlackSet) != 0);
         } else assert(false);

         // Remove the piece that you jumped over, and save it.
         Piece *removedPiece = Take(jumpedOverCell, -mWhoseMove);
         mCapturedPieces.push_back(removedPiece);
      }
   }

   // Add the piece to its final destination
   Cell *destinationCell =
    GetCell((*locs)[locs->size()-1].first, (*locs)[locs->size()-1].second);
   Put(pieceToMove, destinationCell);

   // If you just Put() into the back row, then this is a "king me" move.
   // Set the flag for it, and add this cell to the mKingSet bitmask.
   if ((mWhoseMove == kBlack && ((destinationCell->mask & mWhiteBackSet) != 0))
    || (mWhoseMove == kWhite && ((destinationCell->mask & mBlackBackSet) != 0))) {
      mKingSet |= destinationCell->mask;
      castedMove->mIsKingMeMove = true;
   }

   // Assert that the two bitmasks don't have any pieces in common.
   assert((mBlackSet & mWhiteSet) == 0);

   // Change whose move it is and keep a track of this move in mMoveHist
   mMoveHist.push_back(move);
   mWhoseMove = -mWhoseMove;

   // Assert that the two bitmasks don't have any pieces in common.
   assert((mBlackSet & mWhiteSet) == 0);
}

void CheckersBoard::UndoLastMove() {
   // Load up the last move in mMoveHist.
   CheckersMove *moveToUndo = dynamic_cast<CheckersMove *>(mMoveHist.back());
   Cell *originCell = GetCell(moveToUndo->mLocs[0]);
   Cell *destCell = GetCell(moveToUndo->mLocs[moveToUndo->mLocs.size()-1]);
   Piece *pieceToMove = NULL;

   // Switch whose move it is (early, so that you're taking away the turn
   // that happened BEFORE this current turn)
   mWhoseMove = -mWhoseMove;

   // First, regardless of if this move is a jump move or not, Take the 
   // ending location away (remembering if it was a King or not) and Put the 
   // starting location back in.
   pieceToMove = Take(destCell, mWhoseMove);
   Put(pieceToMove, originCell);

   // If you're undoing a "king me" move, then un-King the piece that you
   // Put back in.
   if (moveToUndo->mIsKingMeMove) {
      mKingSet &= ~(originCell->mask);
   }
   
   // If you're undoing a jump move, then Put each of the moves that you 
   // captured back in.
   if (moveToUndo->mIsJumpMove) {
      int numberOfPiecesToPutBack = moveToUndo->mLocs.size() - 1;

      for (int i = 0; i < numberOfPiecesToPutBack; i++) {
         Piece *pieceToPutBack = mCapturedPieces.back();
         mCapturedPieces.pop_back();
         Cell *cellToPutItIn = GetCell(pieceToPutBack->loc);
         Put(pieceToPutBack, cellToPutItIn);

         // Delete the Piece object once you put it back into the board.
         delete pieceToPutBack;
      }
   }

   // Destroy history of the move
   delete moveToUndo;
   mMoveHist.pop_back();
}

void CheckersBoard::GetAllMoves(list<Move *> *uncastMoves) const {
   list<CheckersMove *> *castedMoves = reinterpret_cast<list<CheckersMove *>*>(uncastMoves);
   char row;
   unsigned int col;
   bool canJumpAtLeastOnce = false;

   assert(uncastMoves->size() == 0 && castedMoves->size() == 0);

   // If the game is over, don't construct a list
   if (mBlackCount == 0 || mWhiteCount == 0)
      return;

   for (row = 'A'; row <= 'H'; row++) {
      for (col = ((row-'A')%2) + 1; col <= kWidth; col += 2) {
         Cell *cell = GetCell(row, col);
         assert(cell != NULL);

         for (int dir = 0; dir < kSqr; dir++) {
            // If a piece can move in a particular direction, then do it.
            // It can't possibly jump a piece in that direction.
            if (!canJumpAtLeastOnce && CanMove(cell, dir)) {
               // Add a non-jump move in this direction
               CheckersMove::LocVector locs;
               locs.push_back(cell->loc);
               locs.push_back(cell->neighborCells[dir]->loc);
               CheckersMove *newMove = new CheckersMove(locs, false);
               castedMoves->push_back(newMove);
            }
            else if (CanJump(cell, dir)) {
               // At this point, you're guaranteed to have at least one
               // possible jump that you can make.  Run a DFS on the possible 
               // multiple jumps that you can take.

               // Before you do that, remove any other "non-jump" moves that
               // you added before you add "jump" moves.
               if (!canJumpAtLeastOnce) {
                  for (list<CheckersMove *>::iterator listIter = castedMoves->begin();
                   listIter != castedMoves->end(); listIter++) {
                      delete *listIter;
                  }
                  castedMoves->clear();
                  canJumpAtLeastOnce = true;
               }

               // Add the starting location to any possible jumps, since they
               // all would start from this location.
               CheckersMove::LocVector locs;
               locs.push_back(cell->loc);
               MultipleJumpDFS(castedMoves, locs, cell);
            }
         }
      }
   }
}

void CheckersBoard::MultipleJumpDFS(list<CheckersMove *> *moves, 
 CheckersMove::LocVector locs, Cell *cell) const {
   Cell *destCell = NULL;
   bool foundDeeperJumpBranch = false;

   // If moving this to this cell is a "king me" move, then immediately 
   // cut off this branch from having any deeper branches (set
   // foundDeeperJumpBranch to false and break before getting to the
   // recursive call).
   if ((cell->mask & mKingSet) == 0) {
      if ((mWhoseMove == kBlack && (cell->mask & mWhiteBackSet))
         || (mWhoseMove == kWhite && (cell->mask & mBlackBackSet))) {
         CheckersMove *newMove = new CheckersMove(locs, true);
         moves->push_back(newMove);
         return;
      }
   }
//    // Otherwise, check all possible routes as usual.
//    else {
      for (int dir = 0; dir < kSqr; dir++) {
         if (CanJump(cell, dir)) {
            // You've found a multiple jump route here.  Set up for your recursive
            // call to keep searching deeper.
            destCell = cell->neighborCells[dir]->neighborCells[dir];
            foundDeeperJumpBranch = true;

            // Temporarily move this piece from the old location to the new 
            // location, and remove the piece that you jumped over.  Undo all of 
            // this after the recursive call.
            Piece *oldLocation = HalfTake(cell, mWhoseMove);
            Piece *capturedPiece = HalfTake(cell->neighborCells[dir], -mWhoseMove);
            HalfPut(oldLocation, destCell);

            // Add the cell that you would jump over into to the LocVector
            // that you're constructing for this [multiple] jump.
            locs.push_back(destCell->loc);

            // Recursive call, using the appended LocVector and the new location
            // that you would jump into.
            MultipleJumpDFS(moves, locs, destCell);

            // Remove this most recent Location from the LocVector, since you're
            // going to be reusing this LocVector as you recurse.
            locs.pop_back();

            // Clean up after the temporary jump move.
            Piece *newLocation = HalfTake(destCell, mWhoseMove);
            delete newLocation;
            HalfPut(capturedPiece, cell->neighborCells[dir]);
            HalfPut(oldLocation, cell);
            delete capturedPiece;
            delete oldLocation;
         }
      }
   // }

   // If you get to a branch that is cut off from having any deeper branches
   // in any direction, then you've found a jump (or multiple jump) that doens't
   // stop halfway through completion.  Add it to the list of moves.
   if (!foundDeeperJumpBranch) {
      CheckersMove *newMove = new CheckersMove(locs, true);
      moves->push_back(newMove);
   }
}

Board::Move *CheckersBoard::CreateMove() const {
   return new CheckersMove(CheckersMove::LocVector(), false);
}


Board *CheckersBoard::Clone() const {
   // [Staley] Think carefully about this one.  You should be able to do it in just
   // [Staley] 5-10 lines.  Don't do needless work
   CheckersBoard *boardCopy = new CheckersBoard(*this);

   list<Move *>::iterator moveHistIter;

   boardCopy->mMoveHist.clear();
   for (moveHistIter = boardCopy->mMoveHist.begin(); 
    moveHistIter != boardCopy->mMoveHist.end(); moveHistIter++) {
      *moveHistIter = (*moveHistIter)->Clone();
   }

   return boardCopy;
}


Board::Key *CheckersBoard::GetKey() const {
   // Leave this empty when you first turn in Milestone 1 -- Bender doesn't
   // check this at all.  That is, be sure to start getting rolling in handins 
   // BEFORE implementing this.
   return NULL;
}

void *CheckersBoard::GetOptions() {
   // The caller of this method owns the object that is returned here.
   return new Rules(mRules);
}

void CheckersBoard::SetOptions(const void *opts) {
   // The caller of this method owns the object that is returned here.
   mRules = *reinterpret_cast<const Rules *>(opts);
}

istream &CheckersBoard::Read(istream &is) {
   // TODO: 
   return is;
}

ostream &CheckersBoard::Write(ostream &os) const { 
   // TODO:
   return os;
}
