#include <windows.h>
#include <GL/gl.h>
#include "glext.h"
#include "config.h"

#define FRAME_SIZE 3*XRES*YRES
#define QUEUE_SIZE 8

/**
 * Almost textbook producer/consumer implementation:
 * https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem
 * The main thread renders the frames and copies from the GPU into a queue.
 * The consumer thread pipes them into the standard input of ffmpeg.
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
    frame_t frames[QUEUE_SIZE];
} frame_queue_t;

static frame_queue_t frameQueue;

static HANDLE ffmpegStdinWrite;
static PROCESS_INFORMATION ffmpegPi;

static HANDLE pipeThread;
static DWORD pipeThreadId;

static HANDLE captureFailEvent = NULL; // signaled if consumer fails
static volatile LONG captureFailed = 0; // 0 ok, 1 failed


static int write_all(HANDLE hnd, const void* data, DWORD bytes) {
    const BYTE* p = (const BYTE*)data;
    while (bytes > 0) {
        DWORD nbWritten = 0;
        if (!WriteFile(hnd, p, bytes, &nbWritten, NULL)) {
            return 0; // error
        }
        p += nbWritten;
        bytes -= nbWritten;
    }
    return 1; // success
}

static void start_ffmpeg(HWND hwnd) {
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdinRead = NULL;
    CreatePipe(&stdinRead, &ffmpegStdinWrite, &sa, 0);

    SetHandleInformation(ffmpegStdinWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFO si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = stdinRead;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    char cmd[1024];
    #ifdef SOUND
    wsprintf(cmd,
        "ffmpeg -y "
        "-f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i - "
        "-i \".\\audio.mp3\" "
        "-map 0:v:0 -map 1:a:0 "
        "-vf vflip "
        "-c:v libx264 -preset ultrafast -crf 18 -pix_fmt yuv420p "
        "-c:a aac -b:a 192k "
        "-shortest "
        "\".\\capture.mp4\"",
        XRES, YRES, CAPTURE_FRAMERATE);
    #else
    wsprintf(cmd,
        "ffmpeg -y "
        "-f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i - "
        "-c:v libx264 -preset ultrafast -crf 18 -pix_fmt yuv420p "
        "\".\\capture.mp4\"",
        XRES, YRES, CAPTURE_FRAMERATE);
    #endif

    BOOL ok = CreateProcess(
        NULL,
        cmd,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &ffmpegPi);
    
    if (!ok) {
        MessageBox(hwnd, "Failed to start ffmpeg for video capture.", "Error", MB_OK);
        ExitProcess(1);
    }

    CloseHandle(stdinRead);
}

static void finish_ffmpeg(HWND hwnd) {
    CloseHandle(ffmpegStdinWrite); // EOF to ffmpeg
    WaitForSingleObject(ffmpegPi.hProcess, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(ffmpegPi.hProcess, &exitCode);
    if (exitCode != 0) {
        MessageBox(hwnd, "Failed to encode video capture.", "Error", MB_OK);
        ExitProcess(1);
    }

    CloseHandle(ffmpegPi.hThread);
    CloseHandle(ffmpegPi.hProcess);
}

static DWORD WINAPI pipe_queued_frames() {
    // Consumer
    while(1) {
        WaitForSingleObject(frameQueue.fullSlotsSem, INFINITE);

        int frameIndex = frameQueue.last;
        frameQueue.last = (frameQueue.last + 1) % QUEUE_SIZE;

        frame_t* frame = &frameQueue.frames[frameIndex];
        if(frame->id < 0) { // poison pill
            ReleaseSemaphore(frameQueue.freeSlotsSem, 1, NULL);
            return 0;
        }
        if(!write_all(ffmpegStdinWrite, frame->pixels, FRAME_SIZE)) {
            InterlockedExchange(&captureFailed, 1);
            SetEvent(captureFailEvent);
            ReleaseSemaphore(frameQueue.freeSlotsSem, 1, NULL);
            return 1;
        }

        ReleaseSemaphore(frameQueue.freeSlotsSem, 1, NULL);
    }
    return 0;
}

void start_capture(HWND hwnd) {
    start_ffmpeg(hwnd);

    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    captureFailEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    captureFailed = 0;

    frameQueue.next = frameQueue.last = 0;
    frameQueue.freeSlotsSem = CreateSemaphore(NULL, QUEUE_SIZE, QUEUE_SIZE, NULL);
    frameQueue.fullSlotsSem = CreateSemaphore(NULL, 0, QUEUE_SIZE, NULL);

    pipeThread = CreateThread(NULL, 0, pipe_queued_frames, NULL, 0, &pipeThreadId);
    if(pipeThread == NULL) {
        MessageBox(hwnd, "Failed to create pipe thread.", "Error", MB_OK);
        ExitProcess(1);
    };
}

void capture_frame(int frameId, HWND hwnd) {
    // Producer
    HANDLE waits[2] = { frameQueue.freeSlotsSem, captureFailEvent };
    DWORD w = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    if (w == WAIT_OBJECT_0 + 1) {
        MessageBox(hwnd, "Failed to pipe frame to ffmpeg.", "Error", MB_OK);
        ExitProcess(1);
    }
    if (w == WAIT_FAILED) {
        MessageBox(hwnd, "Failed capture frame wait.", "Error", MB_OK);
        ExitProcess(1);
    }
    if (w != WAIT_OBJECT_0) {
        MessageBox(hwnd, "Unexpected frame capture fail.", "Error", MB_OK);
        ExitProcess(1);
    }

    frame_t* frame = &frameQueue.frames[frameQueue.next];
    frame->id = frameId;
    glReadPixels(0, 0, XRES, YRES, GL_RGB, GL_UNSIGNED_BYTE, frame->pixels);
    frameQueue.next = (frameQueue.next + 1) % QUEUE_SIZE;

    ReleaseSemaphore(frameQueue.fullSlotsSem, 1, NULL);
}

void finish_capture(HWND hwnd) {
    HANDLE waits[2] = { frameQueue.freeSlotsSem, captureFailEvent };
    DWORD w = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    if (w == WAIT_OBJECT_0 + 1) {
        MessageBox(hwnd, "Capture failed before shutdown.", "Error", MB_OK);
        ExitProcess(1);
    }
    if (w == WAIT_FAILED) {
        MessageBox(hwnd, "Failed capture finish wait.", "Error", MB_OK);
        ExitProcess(1);
    }
    if (w != WAIT_OBJECT_0) {
        MessageBox(hwnd, "Unexpected capture finish wait result.", "Error", MB_OK);
        ExitProcess(1);
    }

    frame_t* frame = &frameQueue.frames[frameQueue.next];
    frame->id = -1; // poison pill
    frameQueue.next = (frameQueue.next + 1) % QUEUE_SIZE;
    ReleaseSemaphore(frameQueue.fullSlotsSem, 1, NULL);

    WaitForSingleObject(pipeThread, INFINITE);
    CloseHandle(pipeThread);

    CloseHandle(frameQueue.freeSlotsSem);
    CloseHandle(frameQueue.fullSlotsSem);

    finish_ffmpeg(hwnd);
}

void save_audio(short* buffer, DWORD bytes, HWND hwnd) {
    // Save raw buffer to file
    HANDLE file = CreateFile(
        ".\\audio.raw",
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

    if (!write_all(file, buffer, bytes)) {
        CloseHandle(file);
        DeleteFile(".\\audio.raw");
        MessageBox(hwnd, "Failed to write raw audio file.", "Error", MB_OK);
        ExitProcess(1);
    }

    CloseHandle(file);

    // Run ffmpeg to convert raw to mp3
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = {0};

    char cmd[1024];
    wsprintf(cmd,
        "ffmpeg -y "
        "-f s16le -ar 44100 -ac 2 -i \"%s\" "
        "-c:a libmp3lame -q:a 2 "
        "\"%s\"",
        ".\\audio.raw",
        ".\\audio.mp3"
    );

    BOOL ok = CreateProcess(
        NULL, cmd,
        NULL, NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &pi
    );

    if (!ok) {
        DeleteFile(".\\audio.raw");
        MessageBox(hwnd, "Failed to start ffmpeg for MP3 encoding.", "Error", MB_OK);
        ExitProcess(1);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    DeleteFile(".\\audio.raw");

    if (exitCode != 0) {
        MessageBox(hwnd, "ffmpeg MP3 encoding failed.", "Error", MB_OK);
        ExitProcess(1);
    }
}