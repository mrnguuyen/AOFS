#include <stdio.h>
#include <time.h>


int main(){
    int numfiles = 100;
	FILE *opening[numfiles];
    clock_t timer;

    timer = clock();
    for(int i = 1; i <= numfiles; i++){
        char filename[20];
        sprintf(filename, "file%d.text", i);
        opening[i] = fopen(filename, "w+");
    }
    timer = clock() - timer;
    double time_spent = ((double)timer)/CLOCKS_PER_SEC;

    printf("Creating 100 files took %f seconds to execute \n", time_spent); 

    // Writing to a file
    timer = clock();
    fprintf(opening[1],"Hello world!");     
    fclose(opening[1]);
    timer = clock() - timer;

    double time_spent1 = ((double)timer)/CLOCKS_PER_SEC;
    printf("Writing to a file took %f seconds to execute \n", time_spent1); 

    //Reading from a file
    FILE *fp;

    fp = fopen("read.txt", "w+");
    fprintf(fp,"Hello world!");

    char line[256];
    //rewind(opening[1]);
    fseek(fp, 0, SEEK_SET);

    timer = clock();
    if(fgets(line, sizeof(line), fp) != NULL) {
        printf("%s\n", line);
    } else {
        printf("failed to read");
    }
    timer = clock() - timer;

    double time_spent2 = ((double)timer)/CLOCKS_PER_SEC;
    printf("Reading from file took %f seconds to execute \n", time_spent2); 

    //Create a file larger than 4KB
    FILE *fp1;

    fp1 = fopen("large.txt", "w+");
    char* text = "Hello World"; 

    for(int i = 0; i < 500; i++){
        fprintf(fp1, text);
    }

	return 0;
}