.# Parallel

-----------------------------------------------------------------------------------------------------------
P: 10/02/2018
# printRows.c
>Φτιάχνει έναν 2D array στην process 0 και μετα κάνει send κάθε γραμμή στις υπόλοιπες processes 
>Στο τέλος όλες οι processes εκτυπώνουν τις γραμμές τους
!!!! ΠΡΟΣΟΧΗ: πρέπει να το τρέξεις με παράμετρο 5 processes,ώστε το comm_size να είναι 5, γιατί είναι "καρφωτό" αλλιώς θα έχει πρόβλημα

------------------------------------------------------------------------------------------------------------
S: 11/02/2018
# filterGray.c
>Φιλτράρει το gray raw αρχείο 10 φορές.
>Πρέπει να βρίσκεται στον ίδιο φάκελο το waterfall_grey_1920_2520.raw

------------------------------------------------------------------------------------------------------------
P: 11/02/2018
# printColumns.c
>Φτιάχνει έναν ΣΤΑΤΙΚΟ 2D array στην process 0 και μετα κάνει send κάθε στήλη στις υπόλοιπες processes 
>Στο τέλος όλες οι processes εκτυπώνουν τις στηλες τους
!!!! ΠΡΟΣΟΧΗ: 1)πρέπει να το τρέξεις με παράμετρο 5 processes,ώστε το comm_size να είναι 5, γιατί είναι "καρφωτό" αλλιώς θα έχει πρόβλημα
	      2)Λειτουργει μονο για στατικο 2D array ωστε να ειναι συνεχομενες οι θεσεις μνημης

#split2Darray.c
>Γεμίζει έναν ΣΤΑΤΙΚΟ 2D array στην process 0 και μετά μέσω της Scatterv στέλνει έναν subarray σε κάθε process
>ΟΛΕΣ οι τιμές των πινάκων έχουν μπει "καρφωτά" , για τον global πίνακα 6χ6
!!!! ΠΡΟΣΟΧΗ: Πρέπει το πρόγραμμα να τρέξει με παράμετρο 4 processes.

------------------------------------------------------------------------------------------------------------
P: 13/02/2018
# 16x16_Split_Send.c
>Φτιάχνει έναν ΣΤΑΤΙΚΟ 2D 16x16 array στην process 0.
>Με Scatterv μοιράζει subarrays στα processes.
>Τα processes ανταλλάσουν rows,corners μεταξύ τους και στο τέλος τις εκτυπώνουν.
!!!! ΠΡΟΣΟΧΗ: Πρέπει το πρόγραμμα να τρέξει με παράμετρο 4 processes.

------------------------------------------------------------------------------------------------------------
