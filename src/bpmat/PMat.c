#include <stdio.h>
#include "PMat.h"
#include "FElibrary.h"

/*
  Parallel matrix implementation

  Copyright (c) 2010 Graeme Kennedy. All rights reserved.
  Not for commercial purposes.
*/

/*!
  The set up for the parallel block-CSR matrix.

  The parallel matrix is split into two parts that are identified
  in the initialization. The diagonal matrix and the off-diagonal 
  matrix. The off-diagonal matrix corresponds to the coupling 
  terms to the external-interface unknowns. The internal-interface
  unknowns must be ordered last on each process. External-interface
  unknowns can only be coupled to other interface-unknowns (either
  external or internal). Thus the global matrix can be represented as
  follows

  A_i = [ B_i, F_i ; G_i, C_i ]
  u_i = [ x_i, y_i ]^{T}

  On each process the unknowns are divided into internal-variables x_i, 
  and internal-iterface variables y_i.
  
  Each domain is coupled to other domains only through the interface
  variables y_i.

  A_i u_i + P * E_{ij} y_j = b_i

  where P = [ 0, I_{size(y_i)} ]^{T}

  The matrix structure outlined above can be exploited to achieve 
  efficient and effective parallel preconditioning.
*/
PMat::PMat( TACSVarMap *_rmap,
	    BCSRMat *_Aloc, BCSRMat *_Bext,
	    TACSBVecDistribute *_ext_dist,
	    TACSBcMap *_bcs ){
  init(_rmap, _Aloc, _Bext, _ext_dist, _bcs);
}

PMat::PMat(){
  rmap = NULL;
  Aloc = NULL;
  Bext = NULL;
  bcs = NULL;
  ext_dist = NULL;
  ctx = NULL;
  x_ext = NULL;
  N = 0; Nc = 0; Np = 0; bsize = 0;
}

/*
  Initialize the PMat object
*/
void PMat::init( TACSVarMap *_rmap,
		 BCSRMat *_Aloc, BCSRMat *_Bext,
		 TACSBVecDistribute *_ext_dist,
		 TACSBcMap *_bcs ){
  // Set the variable map and the local matrix components
  rmap = _rmap;
  Aloc = _Aloc;
  Bext = _Bext;
  rmap->incref();
  Aloc->incref();
  Bext->incref();
  
  bcs = _bcs;   
  if (bcs){ bcs->incref(); }

  // No external column map
  ext_dist = NULL;
  x_ext = NULL;

  N = Aloc->getRowDim();
  if (N != Aloc->getColDim()){
    fprintf(stderr, "PMat error: Block-diagonal matrix must be square\n");
    return;
  }

  Nc = Bext->getRowDim();
  Np = N-Nc;
  if (Nc > N){    
    fprintf(stderr, "PMat error: Block-diagonal matrix must be square\n");
    return;
  }

  int rank;
  MPI_Comm_rank(rmap->getMPIComm(), &rank);
  printf("[%d] PMat diagnostics: N = %d, Nc = %d\n", rank, N, Nc);

  // Copy the distribution array vector
  ext_dist = _ext_dist;
  ext_dist->incref();
  if (Bext->getColDim() != ext_dist->getDim()){
    fprintf(stderr, "PMat error: Dimensions of external variables and \
external block matrix do not match\n");
    return;
  }

  bsize = Aloc->getBlockSize();
  if (Bext->getBlockSize() != bsize){
    fprintf(stderr, "Block sizes do not match\n");
    return;
  }

  // Create a context for distributing the non-local unknowns
  ctx = ext_dist->createCtx(bsize);
  ctx->incref();

  // Allocate the external array
  int len = bsize*ext_dist->getDim();
  x_ext = new TacsScalar[ len ];
  memset(x_ext, 0, len*sizeof(TacsScalar));
  ext_offset = bsize*Np;
}

PMat::~PMat(){
  if (rmap){ rmap->decref(); }
  if (Aloc){ Aloc->decref(); }
  if (Bext){ Bext->decref(); }
  if (bcs){ bcs->decref(); }
  if (ext_dist){ ext_dist->decref(); }
  if (ctx){ ctx->decref(); }
  if (x_ext){ delete [] x_ext; }
}

/*!
  Determine the local dimensions of the matrix - the diagonal part
*/
void PMat::getSize( int *_nr, int *_nc ){
  *_nr = N*bsize;
  *_nc = N*bsize;
}

/*!
  Zero all matrix-entries
*/
void PMat::zeroEntries(){
  Aloc->zeroEntries();
  Bext->zeroEntries();
}

/*!
  Copy the values from the another matrix
*/
void PMat::copyValues( TACSMat *mat ){
  PMat *pmat = dynamic_cast<PMat*>(mat);
  if (pmat){  
    Aloc->copyValues(pmat->Aloc);
    Bext->copyValues(pmat->Bext);
  }
  else {
    fprintf(stderr, "Cannot copy matrices of different types\n");
  }
}

/*!
  Scale the entries in the other matrices by a given scalar
*/
void PMat::scale( TacsScalar alpha ){
  Aloc->scale(alpha);
  Bext->scale(alpha);
}

/*!
  Compute y <- y + alpha * x
*/
void PMat::axpy( TacsScalar alpha, TACSMat *mat ){
  PMat *pmat = dynamic_cast<PMat*>(mat);
  if (pmat){
    Aloc->axpy(alpha, pmat->Aloc);
    Bext->axpy(alpha, pmat->Bext);
  }
  else {
    fprintf(stderr, "Cannot apply axpy to matrices of different types\n");
  }
}

/*!
  Compute y <- alpha * x + beta * y
*/
void PMat::axpby( TacsScalar alpha, TacsScalar beta, TACSMat *mat ){
  PMat *pmat = dynamic_cast<PMat*>(mat);
  if (pmat){
    Aloc->axpby(alpha, beta, pmat->Aloc);
    Bext->axpby(alpha, beta, pmat->Bext);
  }
  else {
    fprintf(stderr, "Cannot apply axpby to matrices of different types\n");
  }
}

/*
  Add a scalar to the diagonal
*/
void PMat::addDiag( TacsScalar alpha ){
  Aloc->addDiag(alpha);
}

// Functions required for solving linear systems
// ---------------------------------------------

/*!
  Matrix multiplication
*/
void PMat::mult( TACSVec *txvec, TACSVec *tyvec ){
  TACSBVec *xvec, *yvec;
  xvec = dynamic_cast<TACSBVec*>(txvec);
  yvec = dynamic_cast<TACSBVec*>(tyvec);

  if (xvec && yvec){
    TacsScalar *x, *y;
    xvec->getArray(&x);
    yvec->getArray(&y);

    ext_dist->beginForward(ctx, x, x_ext);
    Aloc->mult(x, y);
    ext_dist->endForward(ctx, x, x_ext);
    Bext->multAdd(x_ext, &y[ext_offset], &y[ext_offset]);    
  }
  else {
    fprintf(stderr, "PMat type error: Input/output must be TACSBVec\n");
  }
}

/*!
  Access the underlying matrices
*/
void PMat::getBCSRMat( BCSRMat ** A, BCSRMat ** B ){
  *A = Aloc;
  *B = Bext;
}

void PMat::getRowMap( int *_bs, int *_N, int *_Nc ){
  *_bs = bsize;
  *_Nc = Nc;
  *_N = N;
}

void PMat::getColMap( int *_bs, int *_M ){
  *_bs = bsize;
  *_M = N;
}

void PMat::getExtColMap( TACSBVecDistribute ** ext_map ){
  *ext_map = ext_dist;
}

/*!
  Apply the boundary conditions

  For the serial case, this simply involves zeroing the appropriate rows. 
*/
void PMat::applyBCs(){
  if (bcs){
    // Get the MPI rank and ownership range
    int mpi_rank;
    const int *ownerRange;
    MPI_Comm_rank(rmap->getMPIComm(), &mpi_rank);
    rmap->getOwnerRange(&ownerRange);

    // apply the boundary conditions
    const int *local, *global, *var_ptr, *vars;
    const TacsScalar *values;
    int nbcs = bcs->getBCs(&local, &global, &var_ptr, &vars, &values);

    // Get the matrix values
    for ( int i = 0; i < nbcs; i++){
      // Find block i and zero out the variables associated with it
      if (global[i] >= ownerRange[mpi_rank] &&
          global[i] < ownerRange[mpi_rank+1]){

	int bvar  = global[i] - ownerRange[mpi_rank];
	int start = var_ptr[i];
	int nvars = var_ptr[i+1] - start;

	int ident = 1; // Replace the diagonal with the identity matrix
	Aloc->zeroRow(bvar, nvars, &vars[start], ident);

	// Now, check if the variable will be
	// in the off diagonal block (potentially)
	bvar = bvar - (N-Nc);
	if (bvar >= 0){
	  ident = 0;
	  Bext->zeroRow(bvar, nvars, &vars[start], ident);
	}
      }
    }      
  }
}

TACSVec *PMat::createVec(){
  return new TACSBVec(rmap, Aloc->getBlockSize(), bcs);
}

/*!
  Print the matrix non-zero pattern to the screen.
*/
void PMat::printNzPattern( const char *fileName ){
  int mpi_rank;
  const int *ownerRange;
  MPI_Comm_rank(rmap->getMPIComm(), &mpi_rank);
  rmap->getOwnerRange(&ownerRange);

  // Get the sizes of the Aloc and Bext matrices
  int b, Na, Ma;
  const int *rowp, *cols;
  TacsScalar *Avals;
  Aloc->getArrays(&b, &Na, &Ma, &rowp, &cols, &Avals);
  
  int Nb, Mb;
  const int *browp, *bcols;
  TacsScalar *Bvals;
  Bext->getArrays(&b, &Nb, &Mb, &browp, &bcols, &Bvals);

  // Get the map between the global-external 
  // variables and the local variables (for Bext)
  TACSBVecIndices *bindex = ext_dist->getIndices();
  const int *col_vars;
  bindex->getIndices(&col_vars);

  FILE *fp = fopen(fileName, "w");
  if (fp){
    fprintf(fp, "VARIABLES = \"i\", \"j\" \nZONE T = \"Diagonal block %d\"\n", 
            mpi_rank);

    // Print out the diagonal components
    for ( int i = 0; i < Na; i++ ){
      for ( int j = rowp[i]; j < rowp[i+1]; j++ ){
        fprintf(fp, "%d %d\n", i + ownerRange[mpi_rank], 
                cols[j] + ownerRange[mpi_rank]);
      }
    }
    
    if (browp[Nb] > 0){
      fprintf(fp, "ZONE T = \"Off-diagonal block %d\"\n", mpi_rank);
      // Print out the off-diagonal components
      for ( int i = 0; i < Nb; i++ ){
        for ( int j = browp[i]; j < browp[i+1]; j++ ){
          fprintf(fp, "%d %d\n", i + N-Nc + ownerRange[mpi_rank], 
                  col_vars[bcols[j]]);
        }
      }
    }
    
    fclose(fp);
  }
}

const char *PMat::TACSObjectName(){
  return matName;
}

const char *PMat::matName = "PMat";

/*
  Build a simple SOR or Symmetric-SOR preconditioner for the matrix
*/
PSOR::PSOR( PMat *mat, int _zero_guess, 
            TacsScalar _omega, int _iters, 
	    int _isSymmetric ){
  // Get the on- and off-diagonal components of the matrix
  mat->getBCSRMat(&Aloc, &Bext);
  Aloc->incref();
  Bext->incref();

  // Create a vector to store temporary data for the relaxation
  TACSVec *tbvec = mat->createVec();
  bvec = dynamic_cast<TACSBVec*>(tbvec);
  if (bvec){
    bvec->incref();
  }
  else {
    fprintf(stderr, "PSOR type error: Input/output must be TACSBVec\n");
  }

  // Get the number of variables in the row map
  int bsize, N, Nc;
  mat->getRowMap(&bsize, &N, &Nc);

  // Compute the offset to the off-processor terms
  ext_offset = bsize*(N-Nc);

  // Compute the size of the external components
  int ysize = bsize*ext_dist->getDim();
  yext = new TacsScalar[ ysize ];  

  // Get the external column map - a VecDistribute object
  mat->getExtColMap(&ext_dist);
  ext_dist->incref();
  ctx = ext_dist->createCtx(bsize);
  ctx->incref();
  
  // Store the relaxation options 
  zero_guess = _zero_guess;
  omega = _omega;
  iters = _iters;
  isSymmetric = _isSymmetric;
}

/*
  Free the SOR preconditioner
*/
PSOR::~PSOR(){
  Aloc->decref();
  Bext->decref();
  ext_dist->decref();
  ctx->decref();
  delete [] yext;
  if (bvec){ bvec->decref(); }
}

/*
  Factor the diagonal of the matrix
*/
void PSOR::factor(){
  Aloc->factorDiag();
}

/*!
  Apply the preconditioner to the input vector
  Apply SOR to the system A y = x

  The SOR is applied by computing
  Aloc * y = x - Bext * yext

  Then applying the matrix-smoothing for the system of equations,

  Aloc * y = b

  where b = x - Bext * yext
*/
void PSOR::applyFactor( TACSVec *txvec, TACSVec *tyvec ){
  // Covert to TACSBVec objects
  TACSBVec *xvec, *yvec;
  xvec = dynamic_cast<TACSBVec*>(txvec);
  yvec = dynamic_cast<TACSBVec*>(tyvec);

  if (xvec && yvec){
    // Apply the ILU factorization to a vector
    TacsScalar *x, *y, *b;
    yvec->getArray(&y);
    xvec->getArray(&x);
    bvec->getArray(&b);
    
    if (zero_guess){
      yvec->zeroEntries();      
      if (isSymmetric){
        Aloc->applySSOR(x, y, omega, iters);
      }
      else {
        Aloc->applySOR(x, y, omega, iters);
      }
    }
    else {
      // Begin sending the external-interface values
      ext_dist->beginForward(ctx, y, yext);
      
      // Zero entries in the local vector
      bvec->zeroEntries();
      
      // Finish sending the external-interface unknowns
      ext_dist->endForward(ctx, y, yext);
      
      // Compute b[ext_offset] = Bext*yext
      Bext->mult(yext, &b[ext_offset]); 

      // Compute b = xvec - Bext*yext
      bvec->axpby(1.0, -1.0, xvec);         
      
      if (isSymmetric){
        Aloc->applySSOR(b, y, omega, iters);
      }
      else {
        Aloc->applySOR(b, y, omega, iters);
      }
    }
  }
  else {
    fprintf(stderr, "PSOR type error: Input/output must be TACSBVec\n");
  }
}

/*!
  Build the additive Schwarz preconditioner 
*/
AdditiveSchwarz::AdditiveSchwarz( PMat *mat, int levFill, double fill ){
  BCSRMat *B;
  mat->getBCSRMat(&Aloc, &B);
  Aloc->incref();

  Apc = new BCSRMat(mat->getMPIComm(), Aloc, levFill, fill);
  Apc->incref();

  alpha = 0.0; // Diagonal scalar to be added to the preconditioner
}

AdditiveSchwarz::~AdditiveSchwarz(){
  Aloc->decref();
  Apc->decref();
}

void AdditiveSchwarz::setDiagShift( TacsScalar _alpha ){
  alpha = _alpha;
}

void AdditiveSchwarz::factor(){
  Apc->copyValues(Aloc);
  if (alpha != 0.0){ 
    Apc->addDiag(alpha);
  }
  Apc->factor();
}

/*!
  Apply the preconditioner to the input vector

  For the additive Schwarz method that simply involves apply the ILU 
  factorization of the diagonal to the input vector:

  y = U^{-1} L^{-1} x
*/
void AdditiveSchwarz::applyFactor( TACSVec *txvec, TACSVec *tyvec ){
  TACSBVec *xvec, *yvec;
  xvec = dynamic_cast<TACSBVec*>(txvec);
  yvec = dynamic_cast<TACSBVec*>(tyvec);

  if (xvec && yvec){  
    // Apply the ILU factorization to a vector
    TacsScalar *x, *y;
    xvec->getArray(&x);
    yvec->getArray(&y);
    
    Apc->applyFactor(x, y);
  }
  else {
    fprintf(stderr, "AdditiveSchwarz type error: Input/output must be TACSBVec\n");
  }
}

/*!
  Apply the preconditioner to the input vector

  For the additive Schwarz method that simply involves apply the ILU 
  factorization of the diagonal to the input vector:

  y = U^{-1} L^{-1} y
*/
void AdditiveSchwarz::applyFactor( TACSVec *txvec ){
  TACSBVec *xvec;
  xvec = dynamic_cast<TACSBVec*>(txvec);

  if (xvec){  
    // Apply the ILU factorization to a vector
    // This is the default Additive-Scharwz method
    TacsScalar *x;
    xvec->getArray(&x);
    
    Apc->applyFactor(x);
  }
  else {
    fprintf(stderr, "AdditiveSchwarz type error: Input/output must be TACSBVec\n");
  }
}

/*!
  The approximate Schur preconditioner class.
*/
ApproximateSchur::ApproximateSchur( PMat *_mat, int levFill, double fill, 
				    int inner_gmres_iters, double inner_rtol, 
				    double inner_atol ){
  mat = _mat;
  mat->incref();

  BCSRMat *Bext;
  mat->getBCSRMat(&Aloc, &Bext);
  Aloc->incref();

  int rank, size;
  MPI_Comm_rank(mat->getMPIComm(), &rank);
  MPI_Comm_size(mat->getMPIComm(), &size);

  Apc = new BCSRMat(mat->getMPIComm(), Aloc, levFill, fill);
  Apc->incref();
  alpha = 0.0;

  // Check if we're dealing with a serial case here...
  gsmat = NULL;
  rvec = wvec = NULL;
  inner_ksm = NULL;
  
  if (size > 1){
    gsmat = new GlobalSchurMat(mat, Apc);
    gsmat->incref();

    TACSVec *trvec = gsmat->createVec();
    TACSVec *twvec = gsmat->createVec();
    rvec = dynamic_cast<TACSBVec*>(trvec);
    wvec = dynamic_cast<TACSBVec*>(twvec);

    // The code relies on these vectors being TACSBVecs
    if (rvec && twvec){
      rvec->incref();
      wvec->incref();
    }
    else {
      fprintf(stderr, "ApproximateSchur type error: Input/output must be TACSBVec\n");
    }
    
    int nrestart = 0;
    inner_ksm = new GMRES(gsmat, inner_gmres_iters, nrestart);

    inner_ksm->incref();
    inner_ksm->setTolerances(inner_rtol, inner_atol);

    int b, N, Nc;    
    mat->getRowMap(&b, &N, &Nc);
    
    var_offset = N-Nc;
    start = b*(N-Nc);
    end   = b*N; 
  }
}

ApproximateSchur::~ApproximateSchur(){
  Aloc->decref();
  Apc->decref();
  mat->decref();

  if (gsmat){ gsmat->decref(); }
  if (rvec){ rvec->decref(); }
  if (wvec){ wvec->decref(); }
  if (inner_ksm){ inner_ksm->decref(); }
}

void ApproximateSchur::setDiagShift( TacsScalar _alpha ){
  alpha = _alpha;
}

void ApproximateSchur::setMonitor( KSMPrint *ksm_print ){
  if (inner_ksm){
    inner_ksm->setMonitor(ksm_print);
  }
} 	

/*
  Factor preconditioner based on the values in the matrix.
*/
void ApproximateSchur::factor(){
  Apc->copyValues(Aloc);
  if (alpha != 0.0){
    Apc->addDiag(alpha);
  }
  Apc->factor();
}

void ApproximateSchur::printNzPattern( const char *fileName ){
  // Get the sizes of the Aloc and Bext matrices
  int b, Na, Ma;
  const int *rowp;
  const int *cols;
  TacsScalar *Avals;
  Apc->getArrays(&b, &Na, &Ma,
                 &rowp, &cols, &Avals);

  BCSRMat *Aloc, *Bext;
  mat->getBCSRMat(&Aloc, &Bext);

  int Nb, Mb;
  const int *browp;
  const int *bcols;
  TacsScalar *Bvals;
  Bext->getArrays(&b, &Nb, &Mb,
                  &browp, &bcols, &Bvals);

  // Get the map between the global-external 
  // variables and the local variables (for Bext)
  TACSBVecDistribute *ext_dist;
  mat->getExtColMap(&ext_dist);
  TACSBVecIndices *bindex = ext_dist->getIndices();
  const int *col_vars;
  bindex->getIndices(&col_vars);

  TACSVarMap *rmap = mat->getRowMap();
  int mpi_rank;
  MPI_Comm_rank(rmap->getMPIComm(), &mpi_rank);
  const int *ownerRange;
  rmap->getOwnerRange(&ownerRange);

  FILE *fp = fopen(fileName, "w");
  fprintf(fp, "VARIABLES = \"i\", \"j\" \nZONE T = \"Diagonal block %d\"\n", 
          mpi_rank);

  // Print out the diagonal components
  for ( int i = 0; i < Na; i++ ){
    for ( int j = rowp[i]; j < rowp[i+1]; j++ ){
      fprintf(fp, "%d %d\n", i + ownerRange[mpi_rank], 
              cols[j] + ownerRange[mpi_rank]);
    }
  }

  if (browp[Nb] > 0){
    fprintf(fp, "ZONE T = \"Off-diagonal block %d\"\n", mpi_rank);
    // Print out the off-diagonal components
    for ( int i = 0; i < Nb; i++ ){
      for ( int j = browp[i]; j < browp[i+1]; j++ ){
	fprintf(fp, "%d %d\n", i + var_offset + ownerRange[mpi_rank], 
                col_vars[bcols[j]]);
      }
    }
  }

  fclose(fp);
}

/*
  For the approximate Schur method the following calculations are
  caried out:
   
  Compute y = U^{-1} L^{-1} x
  Determine the interface unknowns v_{0} = P * y (the last Nc equations)
  Normalize v_{0} = v_{0}/|| v_{0} ||
  for j = 1, m
  .   Exchange interface unknowns. Obtain v_{j,ext}.
  .   Apply approximate Schur complement. 
  .   v_{j+1} = S^{-1} Bext * v_{j,ext} + v_{j}
  .   For k = 1, j
  .      h_{k,j} = dot(v_{j+1}, v_{j})
  .      v_{j+1} = v_{j} - h_{k,j} v_{k]
  .   h_{j+1,j} = || v_{j+1} ||
  .   v_{j+1} = v_{j+1}/h_{j+1,j}
  Compute z_{m} = argmin || \beta e_{1} - H_{m} z ||_{2}
  V_{m} = [ v_{1}, v_{2}, .. v_{m} ]
  compute w = V_{m} z_{m}
  Exchange interface variables w
  Compute: x <- x - [0,w]
  Compute: y <- U^{-1} L^{-1} x

  -----------------------------------------

  Application of the Schur preconditioner:

  Perform the factorization:
  
  A_{i} = [ L_b          0   ][ U_b  L_b^{-1} E ]
  .       [ F U_b^{-1}   L_s ][ 0    U_s        ]

  Find an approximate solution to:

  [ B  E ][ x_i ]                     [ f_i ]
  [ F  C ][ y_i ] + [ sum F_j y_j ] = [ g_i ]

  Compute the modified RHS:

  g_i' = U_s^{-1} L_s^{-1} ( g_i - F B^{-1} f_i)
  .    = U_s^{-1} L_s^{-1} ( g_i - F U_b^{-1} L_b^{-1} f_i)

  Solve for the interface unknowns (with GMRES):

  y_i + sum  U_s^{-1} L_s^{-1} F_j y_j = g_i'

  Compute the interior unknowns:

  x_i = U_b^{-1} L_b^{-1} ( f_i - E * y_i)  
*/
void ApproximateSchur::applyFactor( TACSVec * txvec, TACSVec * tyvec ){
  TACSBVec *xvec, *yvec;
  xvec = dynamic_cast<TACSBVec*>(txvec);
  yvec = dynamic_cast<TACSBVec*>(tyvec);

  if (xvec && yvec){
    // Apply the ILU factorization to a vector
    TacsScalar *x, *y;
    xvec->getArray(&x);
    yvec->getArray(&y);
    
    if (inner_ksm){
      // y = L_{B}^{-1} x
      // Compute the modified RHS g' = U_s^{-1} L_s^{-1} (g_i - F B^{-1} f_i)
      Apc->applyLower(x, y);
      Apc->applyPartialUpper(&y[start], var_offset);
      
      TacsScalar *r, *w;
      rvec->getArray(&r);
      wvec->getArray(&w);
      memcpy(r, &y[start], (end-start)*sizeof(TacsScalar));
      
      // Solve the global Schur system: S * w = r
      inner_ksm->solve(rvec, wvec);
      memcpy(&y[start], w, (end-start)*sizeof(TacsScalar));
            
      // Compute the interior unknowns from the interface values
      // x_i = U_b^{-1} L_b^{-1} (f_i - E y_i)  
      // x_i = U_b^{-1} (L_b^{-1} f_i - L_b^{-1} E y_i)  
      Apc->applyFactorSchur(y, var_offset);
    }
    else {
      // y = U^{-1} L^{-1} x
      Apc->applyFactor(x, y);
    }
  }
  else {
    fprintf(stderr, 
            "ApproximateSchur type error: Input/output must be TACSBVec\n");
  }
}

/*!
  The block-Jacobi-preconditioned approximate global Schur matrix. 

  This matrix is used within the ApproximateSchur preconditioner.
*/
GlobalSchurMat::GlobalSchurMat( PMat * mat, BCSRMat * _Apc ){
  Apc = _Apc;
  Apc->incref();

  BCSRMat * Aloc;
  mat->getBCSRMat(&Aloc, &Bext);
  Bext->incref();

  int bsize, N, Nc;
  mat->getRowMap(&bsize, &N, &Nc);

  varoffset = N-Nc;
  nvars = bsize*Nc;
  rmap = new TACSVarMap(mat->getMPIComm(), Nc);

  mat->getExtColMap(&ext_dist);
  ext_dist->incref();
  ctx = ext_dist->createCtx(bsize);
  ctx->incref();

  int xsize = bsize*ext_dist->getDim();
  x_ext = new TacsScalar[ xsize ];  
}

GlobalSchurMat::~GlobalSchurMat(){
  Apc->decref();
  Bext->decref(); 
  ext_dist->decref();
  ctx->decref();
  delete [] x_ext;
}

void GlobalSchurMat::getSize( int * _nr, int * _nc ){
  // Get the local dimensions of the matrix
  *_nr = nvars;
  *_nc = nvars;
}

// Compute y <- A * x 
void GlobalSchurMat::mult( TACSVec * txvec, TACSVec * tyvec ){
  TACSBVec *xvec, *yvec;

  xvec = dynamic_cast<TACSBVec*>(txvec);
  yvec = dynamic_cast<TACSBVec*>(tyvec);

  if (xvec && yvec){
    TacsScalar *x, *y;
    xvec->getArray(&x);
    yvec->getArray(&y);  
    
    // Begin sending the external-interface values
    ext_dist->beginForward(ctx, &x[varoffset], x_ext);
    
    // Finish sending the external-interface unknowns
    ext_dist->endForward(ctx, &x[varoffset], x_ext);
    Bext->mult(x_ext, y);
    
    // Apply L^{-1}
    Apc->applyPartialLower(y, varoffset);
    
    // Apply U^{-1}
    Apc->applyPartialUpper(y, varoffset);
    
    // Finish the matrix-vector product
    yvec->axpy(1.0, xvec);
  }
  else {
    fprintf(stderr, "GlobalSchurMat type error: Input/output must be TACSBVec\n");
  }
}

// Compute y <- Bext * xext 
void GlobalSchurMat::multOffDiag( TACSBVec *xvec, TACSBVec *yvec ){  
  TacsScalar *x, *y;
  xvec->getArray(&x);
  yvec->getArray(&y);  

  // Begin sending the external-interface values
  ext_dist->beginForward(ctx, &x[varoffset], x_ext);
  
  // Finish sending the external-interface unknowns
  ext_dist->endForward(ctx, &x[varoffset], x_ext);
  Bext->mult(x_ext, y);
}

// Return a new Vector
TACSVec * GlobalSchurMat::createVec(){
  return new TACSBVec(rmap, Apc->getBlockSize());
}
