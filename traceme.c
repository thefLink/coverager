#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void func1(){
	printf("func1 called ... \n");
}

void func2(){
	printf("func2 called ... \n");
}

void func3(){
	printf("func3 called ... \n");
}

int main(int argc, char *argv[]){

	if(argc == 1){
		printf("bye\n");
		return 0;
	}

	if(*argv[1] == 'A'){
		func1();
	}else{
		func2();
	}

	func3();


	return 0;
}
