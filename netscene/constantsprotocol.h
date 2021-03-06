#pragma once


/**
 * Reserved NetSceneType,
 * which has been used by framework,
 * and can not be used in your application.
 *
 * You should start from {@code kReservedTypeOffset} + 1
 */
const int kReservedTypeOffset = 10;

const int kNetSceneTypeGetIndexPage     = 0;
const int kNetSceneTypeHelloSvr         = 1;
const int kNetSceneType404NotFound      = 2;
const int kNetSceneTypeGetFavIcon       = 3;


/**
 * Errcode
 */
const int kOK               = 0x00;

/*  below are defined in {@link: unix_socket.h}
#define INVALID_SOCKET      (0x01)
#define CONNECT_FAILED      (0x02)
#define SEND_FAILED         (0x04)
#define RECV_FAILED         (0x08)
#define OPERATION_TIMEOUT   (0x10)
*/

const int kErrSvrUnknown    = 0x20;
const int kErrDatabase      = 0x40;
const int kErrIllegalReq    = 0x80;
const int kErrIllegalResp   = 0x100;    /* used by front-end */
const int kErrFileBroken    = 0x200;
const int kSvrOverload      = 0x400;

