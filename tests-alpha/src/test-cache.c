#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ARRAY_SIZE (1 << 22)  // 4MB 배열 (캐시보다 큼)
#define BLOCK_SIZE 32         // 캐시 블록 크기 (가정)
#define STRIDE (BLOCK_SIZE / sizeof(int))  // 한 블록 단위로 접근

int array[ARRAY_SIZE];

void mshr_test() {
    volatile int sum = 0;  // 컴파일러 최적화 방지

    // 같은 캐시 블록 내에서 여러 번 접근하여 MSHR을 활용
    for (int i = 0; i < ARRAY_SIZE; i += STRIDE) {
        sum += array[i];      // 블록의 첫 번째 요소 접근 (캐시 미스 발생)
        sum += array[i + 1];  // 같은 블록 내 접근 (MSHR 병합 가능)
        sum += array[i + 2];  // 같은 블록 내 접근 (MSHR 병합 가능)
        sum += array[i + 3];  // 같은 블록 내 접근 (MSHR 병합 가능)
    }

    printf("Sum: %d\n", sum);

//    for (int i = 0; i < ARRAY_SIZE; i += STRIDE) {
//        int random_index = rand() % ARRAY_SIZE;
//        sum += array[random_index];  // MSHR 병합이 안 되는 패턴 추가
//    }
}

int main() {
    srand(time(NULL));

    // 배열 초기화 (캐시 로컬리티를 깨뜨림)
    for (int i = 0; i < ARRAY_SIZE; i++) {
        array[i] = rand() % 100;
    }

    // MSHR 활용을 유도하는 접근 패턴 실행
    mshr_test();

    return 0;
}