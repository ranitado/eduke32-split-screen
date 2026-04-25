//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2000, 2003 - Matt Saettler (EDuke Enhancements)
Copyright (C) 2004, 2007 - EDuke32 developers
Copyright (C) 2023, 2023 - Jordon Moss (Aka. StrikerTheHedgefox)

This file is part of EDuke32 / NetDuke32

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
//-------------------------------------------------------------------------

// Lobby chat synchronization using named pipes (Windows), and UNIX domain sockets (UNIX/Linux).

#include "duke3d.h"
#include "chatpipe.h"
#include "sjson.h"

#define CHATPIPE_BUFSIZE 4096
#define CHATPIPE_TIMEOUT 5000

// Colors for the text display
#define CPCOLOR_HEADER "^23"
#define CPCOLOR_USERNAME "^15"
#define CPCOLOR_TEXT "^12"
#define CPCOLOR_JOIN "^08"
#define CPCOLOR_LEAVE "^10"

// Pre-defined, re-usable text
#define CPTEXT_HEADER CPCOLOR_HEADER "[LOBBY] "

// Global vars
char cpTempBuf[CHATPIPE_BUFSIZE];
bool pipeActive = false;

// Platform-agnostic functions.
static void ChatPipe_WriteToPipe(const char* message);

static bool ChatPipe_ParseJSON(const char* data)
{
	sjson_context* jsonCtx = sjson_create_context(0, 0, NULL);
	sjson_node* jRoot = sjson_decode(jsonCtx, data);
	bool success = false;

	if (jRoot)
	{
		const char* strEvent = sjson_get_string(jRoot, "event", NULL);
		if (!Bstrcasecmp(strEvent, "message"))
		{
			const char* strUser = sjson_get_string(jRoot, "user", NULL);
			const char* strText = sjson_get_string(jRoot, "text", NULL);
			
			snprintf(cpTempBuf, MAXUSERQUOTELEN, CPTEXT_HEADER CPCOLOR_USERNAME "%s: " CPCOLOR_TEXT "%s", strUser, strText);
			G_AddUserQuote(cpTempBuf);
			success = true;
		}
		else if (!Bstrcasecmp(strEvent, "join"))
		{
			const char* strUser = sjson_get_string(jRoot, "user", NULL);

			snprintf(cpTempBuf, MAXUSERQUOTELEN, CPTEXT_HEADER CPCOLOR_USERNAME "%s " CPCOLOR_JOIN "has joined the lobby.", strUser);
			G_AddUserQuote(cpTempBuf);
			success = true;
		}
		else if (!Bstrcasecmp(strEvent, "leave"))
		{
			const char* strUser = sjson_get_string(jRoot, "user", NULL);

			snprintf(cpTempBuf, MAXUSERQUOTELEN, CPTEXT_HEADER CPCOLOR_USERNAME "%s " CPCOLOR_LEAVE "has left the lobby.", strUser);
			G_AddUserQuote(cpTempBuf);
			success = true;
		}

		sjson_delete_node(jsonCtx, jRoot);
	}

#ifdef DEBUGGINGAIDS
	if(success)
		LOG_F(INFO, "%s: JSON parse succeded!", __func__);
	else
		LOG_F(INFO, "%s: JSON parse failed, or unknown message!", __func__);
#endif

	sjson_destroy_context(jsonCtx);
	return success;
}

void ChatPipe_SendMessage(const char* message)
{
	if (!pipeActive)
		return;

	sjson_context* jsonCtx = sjson_create_context(0, 0, NULL);
	sjson_node* jRoot = sjson_mkobject(jsonCtx);
	
	sjson_put_string(jsonCtx, jRoot, "event", "message");
	//sjson_put_string(jsonCtx, jRoot, "user", g_player[myconnectindex].user_name);
	OSD_StripColors(cpTempBuf, message);
	sjson_put_string(jsonCtx, jRoot, "text", cpTempBuf);
	
	char* jEncoded = sjson_stringify(jsonCtx, jRoot, NULL);
	ChatPipe_WriteToPipe(jEncoded);

	sjson_free_string(jsonCtx, jEncoded);
	sjson_delete_node(jsonCtx, jRoot);
	sjson_destroy_context(jsonCtx);
}

#ifdef _WIN32 // Windows implementation
struct {
	HANDLE handle;
	OVERLAPPED overlapRead, overlapWrite;
	DWORD state;
	DWORD bytesRead;
	DWORD bytesToWrite;
	char readBuffer[CHATPIPE_BUFSIZE];
	char writeBuffer[CHATPIPE_BUFSIZE];
	int readFlag;
	int writeFlag;
} Pipe;

HANDLE waitEvent;

enum chatPipeState_t
{
	PIPESTATE_CONNECTING,
	PIPESTATE_READY,
};

static bool ChatPipe_Connect(void)
{
	bool pipeConnected = ConnectNamedPipe(Pipe.handle, &Pipe.overlapRead);

	DWORD error = GetLastError();
	if (pipeConnected) // Overlapped ConnectNamedPipe should return zero
	{
		LOG_F(ERROR, "%s: Chat pipe connection failed with %ld", __func__, error);
		return false;
	}

	if (error == ERROR_IO_PENDING)
	{
		LOG_F(INFO, "%s: Chat pipe connection pending...", __func__);
		return true;
	}
	else if (error == ERROR_PIPE_CONNECTED)
	{
		//a client has already connected between creating the pipe and
		//waiting for a client to connect. This is rare but can happen.
		//When it does, the overlapped wait does not begin, so the overlapped
		//event would not be signalled. We trigger it manually so that the normal
		//workflow continues
		SetEvent(Pipe.overlapRead.hEvent);
		return false;
	}
	else
	{
		LOG_F(ERROR, "%s: Chat pipe connection failed with %ld", __func__, error);
		return false;
	}

	return false;
}

static void ChatPipe_Reconnect()
{
	// Disconnect the pipe instance. 

	if (!DisconnectNamedPipe(Pipe.handle))
	{
		LOG_F(ERROR, "%s: DisconnectNamedPipe failed with %ld.", __func__, GetLastError());
	}

	// Call a subroutine to connect to the new client. 
	Pipe.state = ChatPipe_Connect() ? PIPESTATE_CONNECTING : PIPESTATE_READY;
	Pipe.readFlag = 0;
	Pipe.writeFlag = 0;
}

void ChatPipe_Create(void)
{
	if (pipeActive)
	{
		LOG_F(ERROR, "%s: Already have a chat pipe!", __func__);
		return;
	}

	Pipe.handle = CreateNamedPipe("\\\\.\\pipe\\" APPNAME, 
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, 
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 
		1, 
		CHATPIPE_BUFSIZE,
		CHATPIPE_BUFSIZE,
		CHATPIPE_TIMEOUT,
		NULL);

	if (Pipe.handle == NULL || Pipe.handle == INVALID_HANDLE_VALUE)
	{
		LOG_F(ERROR, "%s: Chat pipe creation failed", __func__);
		return;
	}

	waitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	Pipe.overlapRead.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	Pipe.overlapWrite.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	Pipe.state = ChatPipe_Connect() ? PIPESTATE_CONNECTING : PIPESTATE_READY;
	Pipe.readFlag = 0;
	Pipe.writeFlag = 0;

	pipeActive = true;
}

static void ChatPipe_WriteToPipe(const char* message)
{
	if (!pipeActive)
		return;

	DWORD byteCnt;

	snprintf(Pipe.writeBuffer, sizeof(Pipe.writeBuffer), "%s", message);
	Pipe.bytesToWrite = (strlen(Pipe.writeBuffer)) * sizeof(char);

	bool success = WriteFile(
		Pipe.handle,
		Pipe.writeBuffer,
		Pipe.bytesToWrite,
		&byteCnt,
		&Pipe.overlapWrite);

	DWORD error_code = GetLastError();

	if (!success && error_code != ERROR_IO_PENDING)
	{
		LOG_F(ERROR, "%s: WriteFile failed, error code: %ld", __func__, error_code);
		ChatPipe_Reconnect(); // An error occurred; disconnect from the client. 
		return;
	}

	Pipe.writeFlag++;
}

static void ChatPipe_ReadFromPipe()
{
	bool success = ReadFile(
		Pipe.handle,
		Pipe.readBuffer,
		CHATPIPE_BUFSIZE,
		&Pipe.bytesRead,
		&Pipe.overlapRead);

	DWORD error_code = ::GetLastError();
	if (!success && error_code != ERROR_IO_PENDING)
	{
		LOG_F(ERROR, "%s: ReadFile failed, error code: %ld", __func__, error_code);
		ChatPipe_Reconnect(); // An error occurred; disconnect from the client. 
		return;
	}

	Pipe.readFlag++;

	return;
}

void ChatPipe_OnWriteComplete()
{
	Pipe.writeFlag--;

#ifdef DEBUGGINGAIDS
	LOG_F(INFO, "%s: Write completed. (%ld bytes)", __func__, Pipe.overlapWrite.InternalHigh);
#endif
}

void ChatPipe_OnReadComplete()
{
	Pipe.readFlag--;
	DWORD byteCnt;
	BOOL ol_result = ::GetOverlappedResult(Pipe.handle, &Pipe.overlapRead, &byteCnt, FALSE);
	if (!ol_result) {
		DWORD code = ::GetLastError();
		if (code != ERROR_MORE_DATA) {
			LOG_F(ERROR, "GetOverlappedResult failed, last error: %ld", code);
			ChatPipe_Reconnect();
			return;
		}

		LOG_F(INFO, "%s: Recieved messaged too large! (%ld > %d bytes)", __func__, byteCnt, CHATPIPE_BUFSIZE);
		ChatPipe_ReadFromPipe();
		return;
	}

	Pipe.readBuffer[byteCnt] = '\0';

	if (byteCnt > 0)
	{
		char* p = Pipe.readBuffer;

#ifdef DEBUGGINGAIDS
		LOG_F(INFO, "%s: Got messages! (%ld bytes)", __func__, byteCnt);
#endif

		for (DWORD i = 0; i < byteCnt;) // Bullshit hack because for some reason simultaneous messages get concatenated...
		{
			strcpy(cpTempBuf, p);

#ifdef DEBUGGINGAIDS
			LOG_F(INFO, "DATA: %s", cpTempBuf);
#endif

			if (!ChatPipe_ParseJSON(cpTempBuf))
				break;

			i += strlen(p) + 1;
			p += strlen(p) + 1;
		}
	}

	if(Pipe.state == PIPESTATE_CONNECTING)
	{
		LOG_F(INFO, "%s: Connection established.", __func__);
		Pipe.state = PIPESTATE_READY;
	}
	
	ChatPipe_ReadFromPipe();
}

void ChatPipe_Poll(void) // Called once per game loop
{
	if (!pipeActive)
		return;
	
	HANDLE handles[3] = { waitEvent, Pipe.overlapRead.hEvent, Pipe.overlapWrite.hEvent };

	DWORD waitResult;

	//if (!(waitResult > 0 && waitResult != WAIT_TIMEOUT))
		//return;

	do
	{
		waitResult = WaitForMultipleObjects(3, handles, FALSE, 0);
		//LOG_F(INFO, "%s: waitResult: %ld", __func__, waitResult);
		switch (waitResult)
		{
			case WAIT_OBJECT_0:
				break;
			case WAIT_OBJECT_0 + 1:
				ChatPipe_OnReadComplete();
				break;
			case WAIT_OBJECT_0 + 2:
				ChatPipe_OnWriteComplete();
				break;
			case WAIT_TIMEOUT:
				break;
			default:
				LOG_F(ERROR, "WaitForMultipleObjects failed, error code: %ld", GetLastError());
				break;
		}
	} while (waitResult > 0 && waitResult != WAIT_TIMEOUT);
	
	return;
}
#else // Unix/Linux stuff

static void ChatPipe_WriteToPipe(const char* message)
{
	UNREFERENCED_PARAMETER(message);
}

void ChatPipe_Create(void)
{
	pipeActive = true;
}

void ChatPipe_Poll(void) // Called once per game loop
{
	if (!pipeActive)
		return;
}

#endif