#include "cmph_mkstemp.h"

int cmph_mkstemp(char *tmpl)
{
    #ifdef WIN32
    //windows does not have mkstemp, but does have
    //mktemp. There is a race condition on NFS (see mktemp man), but
    //that is ok.
    int ret = -1;
    int c = 0;
    while (ret == -1)
    {
        tmpl = _mktemp(tmpl);
        ret = _open(tmpl, _O_CREAT | _O_EXCL);
        ++c;
        if (c > 5)
        {
            return -1;
        }
    }
    return ret;
    #else
    return mkstemp(tmpl);
    #endif
}

const char *cmph_get_tmp_dir()
{
#ifdef WIN32
	const char *tmp_dir = "C:\\";
#else
	const char *tmp_dir = "/tmp/";
#endif
	const char *env_tmp_dir = NULL;
	env_tmp_dir = getenv("TMPDIR");
	if (env_tmp_dir) tmp_dir = env_tmp_dir;
	else env_tmp_dir = getenv("TMP");
	if (env_tmp_dir) tmp_dir = env_tmp_dir;
	else env_tmp_dir = getenv("TEMP");
	if (env_tmp_dir) tmp_dir = env_tmp_dir;
	//On WINNT, getenv, might return a reference
	//to another environment variable. glib get away
	//with it using the ExpandEnvironmentStrings() call.
	//We just keep dereferencing it. Might work (hope there
	//are no loops).
#ifdef WIN32
	while (strchr(tmp_dir, '%') != NULL)
	{
		env_tmp_dir = getenv(tmp_dir);
		if (env_tmp_dir) tmp_dir = env_tmp_dir;
	}
#endif
	return tmp_dir;
}

