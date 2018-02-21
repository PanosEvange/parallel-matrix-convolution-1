#include <stdio.h>
#include <mpi.h>     /* For MPI functions, etc */
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NROWS 2520
#define NCOLS 1920

#define TOPLEFT     my_rank-sqrt_comm_sz-1
#define TOP         my_rank-sqrt_comm_sz
#define TOPRIGHT    my_rank-sqrt_comm_sz+1
#define LEFT        my_rank-1
#define RIGHT       my_rank+1
#define BOTTOMLEFT  my_rank+sqrt_comm_sz-1
#define BOTTOM      my_rank+sqrt_comm_sz
#define BOTTOMRIGHT my_rank+sqrt_comm_sz+1


unsigned char ** get2DArray(int rows, int cols){
	unsigned char * space = malloc(rows * cols * sizeof(unsigned char));
	unsigned char ** array = malloc(rows * sizeof(unsigned char*));

	for(int i=0; i<rows; i++){
		array[i] = space+i*cols;
	}

	return array;
}

//Applies a filter to every pixel except
//those that are on the edges
void applyFilter(unsigned char **original,unsigned char **final, int rows, int cols){

	//Apply filter
	for(int i=1; i<rows-1; i++){
		for(int j=1; j<cols-1; j++){
			final[i][j] = original[i-1][j-1]*((double)1/16)
						   + original[i][j-1]*((double)2/16)
						   + original[i+1][j-1]*((double)1/16)
						   + original[i-1][j]*((double)2/16)
						   + original[i][j]*((double)4/16)
						   + original[i+1][j]*((double)2/16)
						   + original[i-1][j+1]*((double)1/16)
						   + original[i][j+1]*((double)2/16)
						   + original[i+1][j+1]*((double)1/16);
		}
	}

	//I should be freeing malloc'd memory here

}

int main(int argc, char *argv[]) {

	double startTime, endTime;

	int comm_sz;               /* Number of processes    */
	int my_rank;               /* My process rank        */

	/* Subarray type */
	MPI_Datatype type,resizedtype;

	MPI_Status status;

	MPI_Datatype column;
	MPI_Datatype row;

	int sqrt_comm_sz;

	unsigned char **array2D; /* original array */
	unsigned char **final2D;

	unsigned char **myArray; /* local array of each process */
	unsigned char **myFinalArray;
	unsigned char **temp;

	unsigned char *topRow;
	unsigned char *bottomRow;

	unsigned char *leftCol;
	unsigned char *rightCol;

	unsigned char leftUpCorn,rightUpCorn,rightDownCorn,leftDownCorn;

	typedef struct neighbor{
		int rank;
		char recieved;	/* 0 -> not been recieved yet, 1 -> recieved  */
		MPI_Request recieveRequest;
		MPI_Request sendRequest;
	} neighbor;

	typedef struct allNeighbors{
		neighbor top;
		neighbor bottom;
		neighbor left;
		neighbor right;
		neighbor leftUp;
		neighbor rightUp;
		neighbor rightDown;
		neighbor leftDown;
	} allNeighbors;

	allNeighbors myNeighbors;

	int counterItems = 0; /* How many items we have recieved */
	int flag = 0;
	int i,j,k;

	/* Arguments of create_subarray */
	int sizes[2]; /* size of original array */
	int subsizes[2]; /* size of subArrays */
	int starts[2] = {0,0}; /* first subArray starts from index [0][0] */

	/* Arguments of Scatterv */
	int *counts; /* How many pieces of data everyone has in units of block */

	int *displs; /* The starting point of everyone's data in the global array, in block extents ( NCOLS/sqrt_comm_sz ints )  */

	/* Start up MPI */
	MPI_Init(&argc, &argv);

	/* Get the number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);

	/* Get my rank among all the processes */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	sqrt_comm_sz = sqrt(comm_sz);

	topRow = malloc( (NCOLS/sqrt_comm_sz) * sizeof(unsigned char) );
	bottomRow = malloc( (NCOLS/sqrt_comm_sz) * sizeof(unsigned char) );

	leftCol = malloc( (NROWS/sqrt_comm_sz) * sizeof(unsigned char) );
	rightCol = malloc( (NROWS/sqrt_comm_sz) * sizeof(unsigned char) );

	array2D = get2DArray(NROWS,NCOLS);
	final2D = get2DArray(NROWS,NCOLS);

	myArray = get2DArray(NROWS/sqrt_comm_sz,NCOLS/sqrt_comm_sz);
	myFinalArray = get2DArray(NROWS/sqrt_comm_sz,NCOLS/sqrt_comm_sz);

	/* Arguments of create_subarray */
	sizes[0] = NROWS;
	sizes[1] = NCOLS;
	subsizes[0] = NROWS/sqrt_comm_sz;
	subsizes[1] = NCOLS/sqrt_comm_sz;

	counts = malloc( comm_sz * sizeof(int) ); /* How many pieces of data everyone has in units of block */
	for ( i=0; i<comm_sz; i++ ){
		counts[i] = 1;
	}

	displs = malloc( comm_sz * sizeof(int) );
	k=0;
	for( i=0; i<sqrt_comm_sz; i++ ){
		for( j=0; j<sqrt_comm_sz; j++ ){
			displs[k] = i*NROWS+j;
			k++;
		}
	}

	/* Creating derived Datatype */
	MPI_Type_create_subarray(2,sizes,subsizes,starts,MPI_ORDER_C,MPI_UNSIGNED_CHAR,&type);
	MPI_Type_create_resized(type,0,(NCOLS/sqrt_comm_sz)*sizeof(unsigned char),&resizedtype);
	MPI_Type_commit(&resizedtype);

	MPI_Type_contiguous(NCOLS/sqrt_comm_sz,MPI_UNSIGNED_CHAR,&row);
	MPI_Type_commit(&row);

	MPI_Type_vector(NROWS/sqrt_comm_sz,1,NCOLS/sqrt_comm_sz,MPI_UNSIGNED_CHAR,&column);
	MPI_Type_commit(&column);


	if (my_rank == 0) {

		/* Fill in array from file*/
		FILE *inputFile = fopen("waterfall_grey_1920_2520.raw", "r");

		for( i=0; i<NROWS; i++ ){
			for( j=0; j<NCOLS; j++ ){
				array2D[i][j] = fgetc(inputFile);
			}
		}

		fclose(inputFile);
	}

	MPI_Scatterv(*array2D,counts,displs, /*process i gets counts[i] types(subArrays) from displs[i] */
				 resizedtype,
				 *myArray,(NROWS/sqrt_comm_sz)*(NCOLS/sqrt_comm_sz),MPI_UNSIGNED_CHAR, /* I'm recieving (NROWS/sqrt_comm_sz)*(NCOLS/sqrt_comm_sz) MPI_UNSIGNED_CHARs into myArray */
				 0,MPI_COMM_WORLD );

	/* Determine my neighbors */
	myNeighbors.top.rank = my_rank-sqrt_comm_sz;
	myNeighbors.bottom.rank = my_rank+sqrt_comm_sz;
	myNeighbors.left.rank = my_rank-1;
	myNeighbors.right.rank = my_rank+1;
	myNeighbors.leftUp.rank = my_rank-sqrt_comm_sz-1;
	myNeighbors.rightUp.rank = my_rank-sqrt_comm_sz+1;
	myNeighbors.rightDown.rank = my_rank+sqrt_comm_sz+1;
	myNeighbors.leftDown.rank = my_rank+sqrt_comm_sz-1;

	/* Which of my neighbors are MPI_PROC_NULL */

	if( my_rank < sqrt_comm_sz ){ //top row

		myNeighbors.top.rank = MPI_PROC_NULL;
		myNeighbors.leftUp.rank = MPI_PROC_NULL;
		myNeighbors.rightUp.rank = MPI_PROC_NULL;

	}

	if( my_rank >= (comm_sz - sqrt_comm_sz) ){ //bottom row

		myNeighbors.bottom.rank = MPI_PROC_NULL;
		myNeighbors.rightDown.rank = MPI_PROC_NULL;
		myNeighbors.leftDown.rank = MPI_PROC_NULL;

	}

	if( (my_rank%sqrt_comm_sz) == 0 ){ //leftmost column

		myNeighbors.left.rank = MPI_PROC_NULL;
		myNeighbors.leftUp.rank = MPI_PROC_NULL;
		myNeighbors.leftDown.rank = MPI_PROC_NULL;

	}

	if( (my_rank%sqrt_comm_sz) == (sqrt_comm_sz-1) ){ //rightmost column

		myNeighbors.right.rank = MPI_PROC_NULL;
		myNeighbors.rightUp.rank = MPI_PROC_NULL;
		myNeighbors.rightDown.rank = MPI_PROC_NULL;

	}

	/* Prepare buffers with default values */
	for( i=0; i<NCOLS/sqrt_comm_sz; i++ ){
		topRow[i] = myArray[0][i];
	}

	for( i=0; i<NCOLS/sqrt_comm_sz; i++ ){
		bottomRow[i] = myArray[(NROWS/sqrt_comm_sz)-1][i];
	}

	for( i=0; i<NROWS/sqrt_comm_sz; i++ ){
		leftCol[i] = myArray[i][0];
	}

	for( i=0; i<NROWS/sqrt_comm_sz; i++ ){
		rightCol[i] = myArray[i][(NCOLS/sqrt_comm_sz)-1];
	}

	leftUpCorn = myArray[0][0];
	rightUpCorn = myArray[0][(NCOLS/sqrt_comm_sz)-1];
	rightDownCorn = myArray[(NROWS/sqrt_comm_sz)-1][(NCOLS/sqrt_comm_sz)-1];
	leftDownCorn = myArray[(NROWS/sqrt_comm_sz)-1][0];

	MPI_Barrier(MPI_COMM_WORLD);
	startTime = MPI_Wtime();

	for(i=0; i<3; i++){

		/* Initialize recieved flags for MPI_Test */
		myNeighbors.top.recieved = 0;
		myNeighbors.bottom.recieved = 0;
		myNeighbors.left.recieved = 0;
		myNeighbors.right.recieved = 0;
		myNeighbors.leftUp.recieved = 0;
		myNeighbors.rightUp.recieved = 0;
		myNeighbors.rightDown.recieved = 0;
		myNeighbors.leftDown.recieved = 0;

		/* Send rows,columns and corners */

		/* Send my topRow to my top process */
		MPI_Isend(myArray[0],1,row,myNeighbors.top.rank,0,MPI_COMM_WORLD,&myNeighbors.top.sendRequest);

		/* Send my leftUpCorn to my leftup process */
		MPI_Isend(&myArray[0][0],1,MPI_UNSIGNED_CHAR,myNeighbors.leftUp.rank,0,MPI_COMM_WORLD,&myNeighbors.leftUp.sendRequest);

		/* Send my leftCol to my left process */
		MPI_Isend(&myArray[0][0],1,column,myNeighbors.left.rank,0,MPI_COMM_WORLD,&myNeighbors.left.sendRequest);

		/* Send my bottomRow to my bottom process */
		MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,myNeighbors.bottom.rank,0,MPI_COMM_WORLD,&myNeighbors.bottom.sendRequest);

		/* Send my leftDownCorn to my leftdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][0],1,MPI_UNSIGNED_CHAR,myNeighbors.leftDown.rank,0,MPI_COMM_WORLD,&myNeighbors.leftDown.sendRequest);

		/* Send my rightUpCorn to my rightup process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,myNeighbors.rightUp.rank,0,MPI_COMM_WORLD,&myNeighbors.rightUp.sendRequest);

		/* Send my rightCol to my right process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,column,myNeighbors.right.rank,0,MPI_COMM_WORLD,&myNeighbors.right.sendRequest);

		/* Send my rightDownCorn to my rightdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,myNeighbors.rightDown.rank,0,MPI_COMM_WORLD,&myNeighbors.rightDown.sendRequest);


		/* Recieve rows,columns and corners */

		/* Recieve topRow from my top process */
		MPI_Irecv(topRow,1,row,myNeighbors.top.rank,0,MPI_COMM_WORLD, &myNeighbors.top.recieveRequest);

		/* Recieve leftUpCorn from my leftup process */
		MPI_Irecv(&leftUpCorn,1,MPI_UNSIGNED_CHAR,myNeighbors.leftUp.rank,0,MPI_COMM_WORLD, &myNeighbors.leftUp.recieveRequest);

		/* Recieve leftCol from my left process */
		MPI_Irecv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,myNeighbors.left.rank,0,MPI_COMM_WORLD, &myNeighbors.left.recieveRequest);

		/* Recieve bottomRow from my bottom process */
		MPI_Irecv(bottomRow,1,row,myNeighbors.bottom.rank,0,MPI_COMM_WORLD, &myNeighbors.bottom.recieveRequest);

		/* Recieve leftDownCorn from my leftdown process */
		MPI_Irecv(&leftDownCorn,1,MPI_UNSIGNED_CHAR,myNeighbors.leftDown.rank,0,MPI_COMM_WORLD, &myNeighbors.leftDown.recieveRequest);

		/* Recieve rightUpCorn from my rightup rocess */
		MPI_Irecv(&rightUpCorn,1,MPI_UNSIGNED_CHAR,myNeighbors.rightUp.rank,0,MPI_COMM_WORLD, &myNeighbors.rightUp.recieveRequest);

		/* Recieve rightCol from my right process */
		MPI_Irecv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,myNeighbors.right.rank,0,MPI_COMM_WORLD, &myNeighbors.right.recieveRequest);

		/* Recieve rightDownCorn from my rightdown process */
		MPI_Irecv(&rightDownCorn,1,MPI_UNSIGNED_CHAR,myNeighbors.rightDown.rank,0,MPI_COMM_WORLD, &myNeighbors.rightDown.recieveRequest);


		/* Apply filter on the inner pixels */
		applyFilter(myArray,myFinalArray,(NROWS/sqrt_comm_sz), (NCOLS/sqrt_comm_sz));

		counterItems = 0;

		/* Check what we have recieved and apply filter to these items */
		while( counterItems < 8 ){

			/* Apply filter on the inner topRow */
			if( myNeighbors.top.recieved == 0 ){

				MPI_Test(&myNeighbors.top.recieveRequest,&flag,&status);
				if( flag ){
					// i=0
					for( j=1; j<(NCOLS/sqrt_comm_sz)-1; j++ ){
						myFinalArray[0][j] = topRow[j-1]*((double)1/16) 		/* leftUpCorn */
										   + myArray[0][j-1]*((double)2/16)		/* left */
										   + myArray[0+1][j-1]*((double)1/16)	/* leftDownCorn */
										   + topRow[j]*((double)2/16)			/* up */
										   + myArray[0][j]*((double)4/16)		/* itself */
										   + myArray[0+1][j]*((double)2/16)		/* down */
										   + topRow[j+1]*((double)1/16)			/* rightUpCorn */
										   + myArray[0][j+1]*((double)2/16)		/* right */
										   + myArray[0+1][j+1]*((double)1/16);	/* rightDownCorn*/
					}
					myNeighbors.top.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}

			/* Apply filter on the inner bottomRow */
			if( myNeighbors.bottom.recieved == 0 ){

				MPI_Test(&myNeighbors.bottom.recieveRequest,&flag,&status);
				if( flag ){
					int s = (NROWS/sqrt_comm_sz)-1;
					for( j=1; j<(NCOLS/sqrt_comm_sz)-1; j++ ){
						myFinalArray[s][j] = myArray[s-1][j-1]*((double)1/16) 	/* leftUpCorn */
										   + myArray[s][j-1]*((double)2/16)		/* left */
										   + bottomRow[j-1]*((double)1/16)		/* leftDownCorn */
										   + myArray[s-1][j]*((double)2/16)		/* up */
										   + myArray[s][j]*((double)4/16)		/* itself */
										   + bottomRow[j]*((double)2/16)		/* down */
										   + myArray[s-1][j+1]*((double)1/16)	/* rightUpCorn */
										   + myArray[s][j+1]*((double)2/16)		/* right */
										   + bottomRow[j+1]*((double)1/16);		/* rightDownCorn*/
					}
					myNeighbors.bottom.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}

			/* Apply filter on the inner leftCol */
			if( myNeighbors.left.recieved == 0 ){

				MPI_Test(&myNeighbors.left.recieveRequest,&flag,&status);
				if( flag ){
					//j=0
					for( j=1; j<(NROWS/sqrt_comm_sz)-1; j++ ){
						myFinalArray[j][0] = leftCol[j-1]*((double)1/16) 		/* leftUpCorn */
										   + leftCol[j]*((double)2/16)			/* left */
										   + leftCol[j+1]*((double)1/16)		/* leftDownCorn */
										   + myArray[j-1][0]*((double)2/16)		/* up */
										   + myArray[j][0]*((double)4/16)		/* itself */
										   + myArray[j+1][0]*((double)2/16)		/* down */
										   + myArray[j-1][0+1]*((double)1/16)	/* rightUpCorn */
										   + myArray[j][0+1]*((double)2/16)		/* right */
										   + myArray[j+1][0+1]*((double)1/16);	/* rightDownCorn*/
					}
					myNeighbors.left.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}

			/* Apply filter on the inner rightCol */
			if( myNeighbors.right.recieved == 0 ){

				MPI_Test(&myNeighbors.right.recieveRequest,&flag,&status);
				if( flag ){
					int s = (NCOLS/sqrt_comm_sz)-1;
					for( j=1; j<(NROWS/sqrt_comm_sz)-1; j++ ){
						myFinalArray[j][s] = myArray[j-1][s-1]*((double)1/16) 	/* leftUpCorn */
										   + myArray[j][s-1]*((double)2/16)		/* left */
										   + myArray[j+1][s-1]*((double)1/16)	/* leftDownCorn */
										   + myArray[j-1][s]*((double)2/16)		/* up */
										   + myArray[j][s]*((double)4/16)		/* itself */
										   + myArray[j+1][s]*((double)2/16)		/* down */
										   + rightCol[j-1]*((double)1/16)		/* rightUpCorn */
										   + rightCol[j]*((double)2/16)			/* right */
										   + rightCol[j+1]*((double)1/16);		/* rightDownCorn*/
					}
					myNeighbors.right.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}

			/* Apply filter on the leftUpCorn */
			if( (myNeighbors.top.recieved == 1) && (myNeighbors.left.recieved == 1) && (myNeighbors.leftUp.recieved == 0) ){

				MPI_Test(&myNeighbors.leftUp.recieveRequest,&flag,&status);
				if( flag ){
					//i=0 and j=0
					myFinalArray[0][0] = leftUpCorn*((double)1/16) 			/* leftUpCorn */
									   + leftCol[0]*((double)2/16)			/* left */
									   + leftCol[1]*((double)1/16)			/* leftDownCorn */
									   + topRow[0]*((double)2/16)			/* up */
									   + myArray[0][0]*((double)4/16)		/* itself */
									   + myArray[0+1][0]*((double)2/16)		/* down */
									   + topRow[1]*((double)1/16)			/* rightUpCorn */
									   + myArray[0][0+1]*((double)2/16)		/* right */
									   + myArray[0+1][0+1]*((double)1/16);	/* rightDownCorn*/

					myNeighbors.leftUp.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}

			/* Apply filter on the rightUpCorn */
			if( (myNeighbors.top.recieved == 1) && (myNeighbors.right.recieved == 1) && (myNeighbors.rightUp.recieved == 0) ){

					MPI_Test(&myNeighbors.rightUp.recieveRequest,&flag,&status);
					if( flag ){
						//i=0
						int r = NCOLS/sqrt_comm_sz - 1 ;

						myFinalArray[0][r] = topRow[r-1]*((double)1/16) 		/* leftUpCorn */
										   + myArray[0][r-1]*((double)2/16)		/* left */
										   + myArray[0+1][r-1]*((double)1/16)	/* leftDownCorn */
										   + topRow[r]*((double)2/16)			/* up */
										   + myArray[0][r]*((double)4/16)		/* itself */
										   + myArray[0+1][r]*((double)2/16)		/* down */
										   + rightUpCorn*((double)1/16)			/* rightUpCorn */
										   + rightCol[0]*((double)2/16)			/* right */
										   + rightCol[1]*((double)1/16);		/* rightDownCorn*/

						myNeighbors.rightUp.recieved = 1;
						counterItems ++;
						flag = 0 ;
					}

			}

			/* Apply filter on the leftDownCorn */
			if( (myNeighbors.bottom.recieved == 1) && (myNeighbors.left.recieved == 1) && (myNeighbors.leftDown.recieved == 0) ){

					MPI_Test(&myNeighbors.leftDown.recieveRequest,&flag,&status);
					if( flag ){

						//j=0
						int r = NROWS/sqrt_comm_sz - 1 ;

						myFinalArray[r][0] = leftCol[r-1]*((double)1/16) 		/* leftUpCorn */
										   + leftCol[r]*((double)2/16)			/* left */
										   + leftDownCorn*((double)1/16)		/* leftDownCorn */
										   + myArray[r-1][0]*((double)2/16)		/* up */
										   + myArray[r][0]*((double)4/16)		/* itself */
										   + bottomRow[0]*((double)2/16)		/* down */
										   + myArray[r-1][0+1]*((double)1/16)	/* rightUpCorn */
										   + myArray[r][0+1]*((double)2/16)		/* right */
										   + bottomRow[1]*((double)1/16);		/* rightDownCorn*/

						myNeighbors.leftDown.recieved = 1;
						counterItems ++;
						flag = 0 ;
					}

			}

			/* Apply filter on the rightDownCorn */
			if( (myNeighbors.bottom.recieved == 1) && (myNeighbors.right.recieved == 1) && (myNeighbors.rightDown.recieved == 0) ){

					MPI_Test(&myNeighbors.rightDown.recieveRequest,&flag,&status);
					if( flag ){
						int r = NROWS/sqrt_comm_sz - 1 ;
						int s = NCOLS/sqrt_comm_sz - 1 ;

						myFinalArray[r][s] = myArray[r-1][s-1]*((double)1/16) 	/* leftUpCorn */
										   + myArray[r][s-1]*((double)2/16)		/* left */
										   + bottomRow[s-1]*((double)1/16)		/* leftDownCorn */
										   + myArray[r-1][s]*((double)2/16)		/* up */
										   + myArray[r][s]*((double)4/16)		/* itself */
										   + bottomRow[s]*((double)2/16)		/* down */
										   + rightCol[r-1]*((double)1/16)		/* rightUpCorn */
										   + rightCol[r]*((double)2/16)			/* right */
										   + rightDownCorn*((double)1/16);		/* rightDownCorn*/

						myNeighbors.rightDown.recieved = 1;
						counterItems ++;
						flag = 0 ;
					}

			}

			/* Wait my Isend */
			MPI_Wait(&myNeighbors.top.sendRequest,&status);
			MPI_Wait(&myNeighbors.leftUp.sendRequest,&status);
			MPI_Wait(&myNeighbors.left.sendRequest,&status);
			MPI_Wait(&myNeighbors.bottom.sendRequest,&status);
			MPI_Wait(&myNeighbors.leftDown.sendRequest,&status);
			MPI_Wait(&myNeighbors.rightUp.sendRequest,&status);
			MPI_Wait(&myNeighbors.right.sendRequest,&status);
			MPI_Wait(&myNeighbors.rightDown.sendRequest,&status);

		}

		/* Swap arrays */
		temp = myArray;
		myArray = myFinalArray;
		myFinalArray = temp;
	}

	endTime = MPI_Wtime();
	printf("%d.\t%f seconds\n", my_rank, endTime-startTime);

	MPI_Gatherv(*myArray,(NROWS/sqrt_comm_sz)*(NCOLS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,*final2D,counts,displs,resizedtype,0,MPI_COMM_WORLD );

	if( my_rank == 0 ){

		FILE *outputFile = fopen("mpiOutput.raw", "w");

		for( i=0; i<NROWS; i++ ){
			for( j=0; j<NCOLS; j++ ){
				//printf("%c", final2D[i][j]);
				fputc(final2D[i][j], outputFile);
			}
		}
		fclose(outputFile);
	}

	free(topRow);
	free(bottomRow);
	free(leftCol);
	free(rightCol);

	free(*array2D);
	free(array2D);
	free(*final2D);
	free(final2D);

	free(*myArray);
	free(myArray);
	free(*myFinalArray);
	free(myFinalArray);

	free(counts);
	free(displs);

	MPI_Type_free(&type);
	MPI_Type_free(&resizedtype);
	MPI_Type_free(&row);
	MPI_Type_free(&column);

	/* Shut down MPI */
	MPI_Finalize();
	return 0;
}
