#include <windows.h>
#include <GL/gl.h>
#include "glext.h"
#include "config.h"
#include "stb_image_write.h"

#define NUM_THREADS 8
#define QUEUE_SIZE 8
#define FRAME_SIZE 3*XRES*YRES

/**
 * Almost textbook producer/consumer implementation:
 * https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem
 * Generating a frame is usually faster than converting it to a PNG and saving
 * it on the disk. Thus the main thread captures frames in a buffer while
 * multiple threads save the buffered frames in parallel.
 */

typedef struct frame_t
{
    int id;
    unsigned char pixels[FRAME_SIZE];
} frame_t;

typedef struct frame_queue_t
{
    int next;
    int last;
    HANDLE freeSlotsSem;
    HANDLE fullSlotsSem;
    HANDLE mutex;
    frame_t frames[QUEUE_SIZE];
} frame_queue_t;

static frame_queue_t frameQueue;

static HANDLE threads[NUM_THREADS];
static DWORD threadIds[NUM_THREADS];

static BOOL shouldTerminate = FALSE;


DWORD WINAPI save_queued_frames();

void init_capture(HWND hwnd) {
    CreateDirectory("capture", NULL);

    #ifdef VIDEO
    frameQueue.next = frameQueue.last = 0;
    frameQueue.freeSlotsSem = CreateSemaphore(NULL, QUEUE_SIZE, QUEUE_SIZE, NULL);
    frameQueue.fullSlotsSem = CreateSemaphore(NULL, 0, QUEUE_SIZE, NULL);
    frameQueue.mutex = CreateMutex(NULL, FALSE, NULL);

    for(int i = 0; i < NUM_THREADS; i++) {
        threads[i] = CreateThread(NULL, 0, save_queued_frames, NULL, 0, &threadIds[i]);
        if(threads[i] == NULL) {
            MessageBox(hwnd, "Failed to create thread.", "Error", MB_OK);
            ExitProcess(1);
        }
    }

    stbi_flip_vertically_on_write(1);

    glReadBuffer(GL_FRONT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    #endif
}

void end_capture(HWND hwnd) {
    #ifdef VIDEO
    shouldTerminate = TRUE; // set termination flag
    // since NUM_THREADS and QUEUE_SIZE could be different, we need to manually
    // signal each thread and wait for it to end before signaling the next one
    for(DWORD nbRunning = NUM_THREADS; nbRunning > 0; nbRunning--) {
        ReleaseSemaphore(frameQueue.fullSlotsSem, 1, NULL);
        DWORD index = WaitForMultipleObjects(nbRunning, threads, FALSE, INFINITE);
        index -= WAIT_OBJECT_0;
        HANDLE last = threads[nbRunning-1];
        threads[nbRunning-1] = threads[index];
        threads[index] = last;
    }
    // WaitForMultipleObjects(NUM_THREADS, threads, TRUE, 1000);

    for(int i = 0; i < NUM_THREADS; i++) {
        CloseHandle(threads[i]);
    }

    CloseHandle(frameQueue.freeSlotsSem);
    CloseHandle(frameQueue.fullSlotsSem);
    CloseHandle(frameQueue.mutex);
    #endif
}

DWORD WINAPI save_queued_frames() {
    // consumer
    char filename[32];
    while(1) {
        WaitForSingleObject(frameQueue.fullSlotsSem, INFINITE);
        if(shouldTerminate) {
            ReleaseSemaphore(frameQueue.fullSlotsSem, 1, NULL);
            break;
        }

        WaitForSingleObject(frameQueue.mutex, INFINITE);
        int frameIndex = frameQueue.last;
        frameQueue.last = (frameQueue.last + 1) % QUEUE_SIZE;
        ReleaseMutex(frameQueue.mutex);

        frame_t* frame = &frameQueue.frames[frameIndex];
        wsprintf(filename, ".\\capture\\frame_%06d.png", frame->id);
        stbi_write_png(filename, XRES, YRES, 3, frame->pixels, 3*XRES);

        ReleaseSemaphore(frameQueue.freeSlotsSem, 1, NULL);
    }
    return 0;
}

void save_frame(int frameId) {
    // producer
    WaitForSingleObject(frameQueue.freeSlotsSem, INFINITE);
    
    frame_t* frame = &frameQueue.frames[frameQueue.next];
    frame->id = frameId;
    glReadPixels(0, 0, XRES, YRES, GL_RGB, GL_UNSIGNED_BYTE, frame->pixels);
    frameQueue.next = (frameQueue.next + 1) % QUEUE_SIZE;

    ReleaseSemaphore(frameQueue.fullSlotsSem, 1, NULL);
}

void save_audio(short* buffer, int bytes, HWND hwnd) {
    HANDLE file = CreateFile(
        ".\\capture\\audio.raw",
        GENERIC_WRITE,
        0,
        NULL, 
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    
    if(file == INVALID_HANDLE_VALUE) {
        MessageBox(hwnd, "Failed to create audio file.", "Error", MB_OK);
        ExitProcess(1);
    }

    DWORD nbWritten;
    if(!WriteFile(file, (LPCVOID)buffer, (DWORD)bytes, &nbWritten, NULL)) {
        MessageBox(hwnd, "Failed to write audio file.", "Error", MB_OK);
        CloseHandle(file);
        ExitProcess(1);
    }

    CloseHandle(file);
}
