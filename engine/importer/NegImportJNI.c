/*  JNI entry points for com.sirbilliams.negatron.GameImporter (see NegImport.h).          */
#include "NegImport.h"
#include <jni.h>
#include <unistd.h>

typedef struct { JNIEnv *env; jobject cb; jmethodID m; } Cb;

static int progress(void *user, int stage, long long done, long long total)
{
	Cb *c = (Cb *)user;
	if (!c->cb) return 0;
	jboolean cancel = (*c->env)->CallBooleanMethod(c->env, c->cb, c->m, (jint)stage, (jlong)done, (jlong)total);
	if ((*c->env)->ExceptionCheck(c->env)) { (*c->env)->ExceptionClear(c->env); return 1; }
	return cancel ? 1 : 0;
}

JNIEXPORT jboolean JNICALL Java_com_sirbilliams_negatron_GameImporter_isIso(JNIEnv *env, jclass cls, jint fd)
{
	(void)env; (void)cls;
	return neg_is_iso(fd) ? JNI_TRUE : JNI_FALSE;
}

/* Returns null on success, otherwise the message for the player. */
JNIEXPORT jstring JNICALL Java_com_sirbilliams_negatron_GameImporter_importIso(JNIEnv *env, jclass cls, jint fd,
	jstring dest, jstring tmp, jobject cb)
{
	(void)cls;
	Cb c = { env, cb, NULL };
	if (cb)
	{
		jclass k = (*env)->GetObjectClass(env, cb);
		c.m = (*env)->GetMethodID(env, k, "onProgress", "(IJJ)Z");
		if (!c.m) { (*env)->ExceptionClear(env); c.cb = NULL; }
	}
	const char *d = (*env)->GetStringUTFChars(env, dest, NULL), *t = (*env)->GetStringUTFChars(env, tmp, NULL);
	char err[512] = "";
	int r = neg_import_iso(fd, d, t, progress, &c, err, sizeof err);
	rmdir(t);
	(*env)->ReleaseStringUTFChars(env, dest, d);
	(*env)->ReleaseStringUTFChars(env, tmp, t);
	return r ? (*env)->NewStringUTF(env, err) : NULL;
}
