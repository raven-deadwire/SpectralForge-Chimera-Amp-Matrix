/* Document-only package maintenance. No audio, settings or user-library code.
 * Copyright (c) 2026 RavenForge Luthier Intelligence. All rights reserved.
 * Build-time fingerprints are the only authority for retiring historical bytes.
 */
#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define MAX_BYTES (64ULL * 1024 * 1024)
#define RESTORE_UPGRADE 1
typedef struct { uint64_t size; const char *sha; } Fingerprint;
typedef struct { const char *path; int retire, always, current; size_t offset, count; } Rule;
#include "document_rules.h"

/* SHA-256 on the exact file bytes. No text decoding/newline normalization. */
typedef struct { uint32_t h[8]; uint64_t length; unsigned char block[64]; size_t used; } SHA;
static const uint32_t sha_k[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static uint32_t rr(uint32_t x, unsigned n) { return (x >> n) | (x << (32-n)); }
static void sha_transform(SHA *s) {
    uint32_t w[64], a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    unsigned i;
    for(i=0;i<16;i++) w[i]=((uint32_t)s->block[i*4]<<24)|((uint32_t)s->block[i*4+1]<<16)|((uint32_t)s->block[i*4+2]<<8)|s->block[i*4+3];
    for(i=16;i<64;i++) { uint32_t x=w[i-15],y=w[i-2]; w[i]=w[i-16]+(rr(x,7)^rr(x,18)^(x>>3))+w[i-7]+(rr(y,17)^rr(y,19)^(y>>10)); }
    for(i=0;i<64;i++) { uint32_t t1=h+(rr(e,6)^rr(e,11)^rr(e,25))+((e&f)^((~e)&g))+sha_k[i]+w[i]; uint32_t t2=(rr(a,2)^rr(a,13)^rr(a,22))+((a&b)^(a&c)^(b&c)); h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void sha_init(SHA *s) {
    static const uint32_t initial[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(s->h,initial,sizeof(initial));s->length=0;s->used=0;
}
static void sha_update(SHA *s,const unsigned char *p,size_t n) {
    s->length+=n;
    while(n) { size_t part=64-s->used;if(part>n)part=n;memcpy(s->block+s->used,p,part);s->used+=part;p+=part;n-=part;if(s->used==64){sha_transform(s);s->used=0;} }
}
static void sha_finish(SHA *s,char out[65]) {
    uint64_t bits=s->length*8;unsigned i;s->block[s->used++]=0x80;
    if(s->used>56){memset(s->block+s->used,0,64-s->used);sha_transform(s);s->used=0;}
    memset(s->block+s->used,0,56-s->used);
    for(i=0;i<8;i++)s->block[63-i]=(unsigned char)(bits>>(i*8));
    sha_transform(s);for(i=0;i<8;i++)snprintf(out+i*8,9,"%08x",s->h[i]);out[64]=0;
}

static int error(const char *message,const char *path) {
    fprintf(stderr,"Document maintenance stopped: %s: %s (%s)\n",message,path?path:"",strerror(errno));return -1;
}
static int safe_component(const char *s) { return *s && strcmp(s,".") && strcmp(s,".."); }
static int open_directory_at(int parent,const char *name,int create,mode_t mode) {
    int fd;
    if(!safe_component(name)){errno=EINVAL;return -1;}
    fd=openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0 && errno==ENOENT && create) {
        if(mkdirat(parent,name,mode)<0) {if(errno!=EEXIST)return -1;}
        else if(fsync(parent)<0)return -1;
        fd=openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    }
    return fd;
}
static int open_absolute_dir(const char *path,int create,mode_t mode) {
    char buffer[PATH_MAX],*part,*state;int fd,next;
    if(path[0]!='/' || strlen(path)>=sizeof(buffer)){errno=EINVAL;return -1;}
    strcpy(buffer,path);fd=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(fd<0)return -1;
    for(part=strtok_r(buffer,"/",&state);part;part=strtok_r(NULL,"/",&state)) {
        next=open_directory_at(fd,part,create,mode);close(fd);if(next<0)return -1;fd=next;
    }
    return fd;
}
/* Returns -2 for absent ancestors; -1 for links/other errors. */
static int parent_for(int root,const char *path,int create,char leaf[PATH_MAX]) {
    char buffer[PATH_MAX],*part,*next,*state;int fd=dup(root),child;
    if(fd<0)return -1;
    if(!*path || path[0]=='/' || strlen(path)>=sizeof(buffer)){close(fd);errno=EINVAL;return -1;}
    strcpy(buffer,path);part=strtok_r(buffer,"/",&state);
    while(part) {
        if(!safe_component(part)){close(fd);errno=EINVAL;return -1;}
        next=strtok_r(NULL,"/",&state);
        if(!next){strcpy(leaf,part);return fd;}
        child=open_directory_at(fd,part,create,0755);close(fd);
        if(child<0)return errno==ENOENT?-2:-1;
        fd=child;part=next;
    }
    close(fd);errno=EINVAL;return -1;
}
static int regular_at(int parent,const char *name) {
    struct stat st;int fd=openat(parent,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(fd<0)return errno==ENOENT?-2:-1;
    if(fstat(fd,&st)<0 || !S_ISREG(st.st_mode) || st.st_nlink!=1 || st.st_size<0 || (uint64_t)st.st_size>MAX_BYTES) {close(fd);errno=EINVAL;return -1;}
    return fd;
}
static int stable(const struct stat *a,const struct stat *b) {
    if(a->st_dev!=b->st_dev || a->st_ino!=b->st_ino || a->st_size!=b->st_size || a->st_nlink!=b->st_nlink)return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec && a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec && a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec && a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec==b->st_mtim.tv_sec && a->st_mtim.tv_nsec==b->st_mtim.tv_nsec && a->st_ctim.tv_sec==b->st_ctim.tv_sec && a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int hash_fd(int fd,uint64_t *size,char sha[65]) {
    struct stat before,after;unsigned char buffer[16384];ssize_t n;SHA state;uint64_t bytes=0;
    if(fstat(fd,&before)<0 || lseek(fd,0,SEEK_SET)<0)return -1;
    if(!S_ISREG(before.st_mode) || before.st_nlink!=1 || before.st_size<0 || (uint64_t)before.st_size>MAX_BYTES){errno=EINVAL;return -1;}
    sha_init(&state);
    while((n=read(fd,buffer,sizeof(buffer)))>0) {bytes+=(uint64_t)n;if(bytes>MAX_BYTES){errno=EFBIG;return -1;}sha_update(&state,buffer,(size_t)n);}
    if(n<0 || fstat(fd,&after)<0 || !stable(&before,&after) || bytes!=(uint64_t)after.st_size){errno=EAGAIN;return -1;}
    sha_finish(&state,sha);*size=bytes;return 0;
}
static int accepted(const Rule *r,uint64_t size,const char *sha) {
    size_t i;for(i=0;i<r->count;i++){const Fingerprint *f=&fingerprints[r->offset+i];if(f->size==size && !strcmp(f->sha,sha))return 1;}return 0;
}
static int write_all(int fd,const void *data,size_t length) {
    const char *p=data;while(length){ssize_t n=write(fd,p,length);if(n<0){if(errno==EINTR)continue;return -1;}if(!n){errno=EIO;return -1;}p+=n;length-=(size_t)n;}return 0;
}
/* Never replace a pre-existing preserved copy or restored user file. */
static int copy_checked(int source,int parent,const char *name,uint64_t size,const char *sha,mode_t mode) {
    int target,existing;char check[65];uint64_t bytes;unsigned char buffer[16384];ssize_t n;
    existing=regular_at(parent,name);
    if(existing>=0){int ok=hash_fd(existing,&bytes,check)==0 && bytes==size && !strcmp(check,sha);close(existing);if(!ok){errno=EEXIST;return -1;}return 0;}
    if(existing!=-2)return -1;
    target=openat(parent,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(target<0)return -1;
    if(lseek(source,0,SEEK_SET)<0){close(target);return -1;}
    while((n=read(source,buffer,sizeof(buffer)))>0)if(write_all(target,buffer,(size_t)n)<0){close(target);return -1;}
    if(n<0 || fsync(target)<0 || fchmod(target,mode&0777)<0){close(target);return -1;}
    close(target);target=regular_at(parent,name);
    if(target<0)return -1;
    if(hash_fd(target,&bytes,check)<0 || bytes!=size || strcmp(check,sha)){close(target);errno=EAGAIN;return -1;}
    close(target);
    if(hash_fd(source,&bytes,check)<0 || bytes!=size || strcmp(check,sha)){errno=EAGAIN;return -1;}
    return fsync(parent);
}
static int auxiliary(const char *docroot,const char *suffix,int create) {
    char path[PATH_MAX];struct stat st;int fd;
    if(snprintf(path,sizeof(path),"%s%s",docroot,suffix)>=(int)sizeof(path)){errno=ENAMETOOLONG;return -1;}
    fd=open_absolute_dir(path,create,0700);
    if(fd<0)return errno==ENOENT?-2:-1;
    if(fstat(fd,&st)<0 || st.st_uid!=geteuid() || (st.st_mode&077)!=0){close(fd);errno=EACCES;return -1;}
    return fd;
}
static int marker(int pending,int create,const char *mode) {
    char buffer[128],expected[128];int fd;ssize_t n;
    snprintf(expected,sizeof(expected),"%s %s\n",RULE_DIGEST,mode);
    if(create) {
        fd=openat(pending,"marker",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(fd>=0){int ok=write_all(fd,expected,strlen(expected))==0 && fsync(fd)==0;close(fd);return ok?0:-1;}
        if(errno!=EEXIST)return -1;
    }
    fd=regular_at(pending,"marker");if(fd<0)return -1;
    n=read(fd,buffer,sizeof(buffer)-1);close(fd);if(n<0)return -1;buffer[n]=0;
    if(strcmp(buffer,expected)){errno=EINVAL;return -1;}return 0;
}
static int pending_mode(int pending,char mode[16]) {
    char buffer[128],digest[65],extra;int fd=regular_at(pending,"marker");ssize_t n;
    if(fd<0)return -1;
    n=read(fd,buffer,sizeof(buffer)-1);close(fd);if(n<0)return -1;buffer[n]=0;
    if(sscanf(buffer,"%64s %15s %c",digest,mode,&extra)!=2 || strcmp(digest,RULE_DIGEST) || (strcmp(mode,"upgrade") && strcmp(mode,"remove"))){errno=EINVAL;return -1;}return 0;
}
static int snapshot(int source,int preserved,const Rule *r,uint64_t size,const char *sha,mode_t mode) {
    char path[PATH_MAX],leaf[PATH_MAX];int parent;
    if(snprintf(path,sizeof(path),"%s/%s",sha,r->path)>=(int)sizeof(path)){errno=ENAMETOOLONG;return -1;}
    parent=parent_for(preserved,path,1,leaf);if(parent<0)return -1;
    {int result=copy_checked(source,parent,leaf,size,sha,mode);close(parent);return result;}
}
static int record_write(int pending,size_t index,uint64_t size,const char *sha,mode_t mode,int flags) {
    char name[64],data[160];int fd;snprintf(name,sizeof(name),"%zu.record",index);
    snprintf(data,sizeof(data),"%llu %s %u %d\n",(unsigned long long)size,sha,(unsigned)(mode&0777),flags);
    fd=openat(pending,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return -1;
    {int ok=write_all(fd,data,strlen(data))==0 && fsync(fd)==0;close(fd);return ok?0:-1;}
}
static int record_read(int pending,size_t index,uint64_t *size,char sha[65],mode_t *mode,int *flags) {
    char name[64],data[192],extra;unsigned long long bytes;unsigned perm;ssize_t n;int fd;size_t i;
    snprintf(name,sizeof(name),"%zu.record",index);fd=regular_at(pending,name);if(fd<0)return fd;
    n=read(fd,data,sizeof(data)-1);close(fd);if(n<0)return -1;data[n]=0;
    if(sscanf(data,"%llu %64s %u %d %c",&bytes,sha,&perm,flags,&extra)!=4 || bytes>MAX_BYTES || perm>0777 || (*flags!=0 && *flags!=RESTORE_UPGRADE) || strlen(sha)!=64){errno=EINVAL;return -1;}
    for(i=0;i<64;i++)if(!((sha[i]>='0'&&sha[i]<='9')||(sha[i]>='a'&&sha[i]<='f'))){errno=EINVAL;return -1;}
    *size=bytes;*mode=(mode_t)perm;return 0;
}
static int restore_record(int root,int preserved,size_t index,uint64_t size,const char *sha,mode_t mode) {
    char backup[PATH_MAX],leaf[PATH_MAX],check[65];int bp,source,parent,target;uint64_t bytes;
    if(snprintf(backup,sizeof(backup),"%s/%s",sha,rules[index].path)>=(int)sizeof(backup)){errno=ENAMETOOLONG;return -1;}
    bp=parent_for(preserved,backup,0,leaf);if(bp<0)return -1;
    source=regular_at(bp,leaf);close(bp);if(source<0)return -1;
    if(hash_fd(source,&bytes,check)<0 || bytes!=size || strcmp(check,sha)){close(source);errno=EINVAL;return -1;}
    parent=parent_for(root,rules[index].path,1,leaf);if(parent<0){close(source);return -1;}
    target=regular_at(parent,leaf);
    if(target>=0) {
        if(hash_fd(target,&bytes,check)<0){close(target);close(parent);close(source);return -1;}
        if(bytes!=size || strcmp(check,sha))fprintf(stderr,"Current document retained; previous bytes remain in preserved copy: %s\n",rules[index].path);
        close(target);
    } else if(target==-2) {
        if(copy_checked(source,parent,leaf,size,sha,mode)<0){close(parent);close(source);return -1;}
        printf("Restored preserved document: %s\n",rules[index].path);
    } else {close(parent);close(source);return -1;}
    close(parent);close(source);return 0;
}
static int cleanup_one(int root,const Rule *r,size_t index) {
    char leaf[PATH_MAX],sha[65],second[65],private_name[96];uint64_t size,other;
    struct stat first,last;int parent,fd,quarantine,captured,ok;
    if(!r->retire || r->always || r->current)return 0;
    parent=parent_for(root,r->path,0,leaf);if(parent==-2)return 0;if(parent<0)return -1;
    fd=regular_at(parent,leaf);if(fd==-2){close(parent);return 0;}if(fd<0){close(parent);return -1;}
    if(hash_fd(fd,&size,sha)<0 || fstat(fd,&first)<0){close(fd);close(parent);return -1;}
    if(!accepted(r,size,sha)){close(fd);close(parent);return 0;}
    snprintf(private_name,sizeof(private_name),".chimera-retire-%ld-%zu",(long)getpid(),index);
    if(mkdirat(parent,private_name,0700)<0){close(fd);close(parent);return -1;}
    quarantine=open_directory_at(parent,private_name,0,0700);
    if(quarantine<0 || renameat(parent,leaf,quarantine,"candidate")<0){if(quarantine>=0)close(quarantine);close(fd);close(parent);return -1;}
    captured=regular_at(quarantine,"candidate");
    ok=captured>=0 && fstat(captured,&last)==0 && first.st_dev==last.st_dev && first.st_ino==last.st_ino && hash_fd(captured,&other,second)==0 && other==size && !strcmp(sha,second) && accepted(r,other,second);
    if(captured>=0)close(captured);
    close(fd);
    if(!ok) {
        /* linkat with flags=0 restores even a raced symlink itself, never its target.
         * It cannot overwrite another concurrent creation at the original path. */
        if(linkat(quarantine,"candidate",parent,leaf,0)==0)unlinkat(quarantine,"candidate",0);
        fprintf(stderr,"Changed document retained at original or recovery path: %s/%s\n",r->path,private_name);
        close(quarantine);unlinkat(parent,private_name,AT_REMOVEDIR);close(parent);errno=EAGAIN;return -1;
    }
    if(unlinkat(quarantine,"candidate",0)<0){close(quarantine);close(parent);return -1;}
    close(quarantine);if(unlinkat(parent,private_name,AT_REMOVEDIR)<0){close(parent);return -1;}
    close(parent);printf("Retired verified legacy document: %s\n",r->path);return 0;
}
static int cleanup(int root) {size_t i;for(i=0;i<rule_count;i++)if(cleanup_one(root,&rules[i],i)<0)return error("cannot retire safely",rules[i].path);return 0;}
static int finish(const char *docroot,const char *requested,int aborting) {
    int pending=auxiliary(docroot,".pending",0),root,preserved,result=0;char mode[16],name[64],sha[65];size_t i;uint64_t size;mode_t perm;int flags,status;
    if(pending==-2){root=open_absolute_dir(docroot,0,0755);if(root<0)return errno==ENOENT?0:error("unsafe document root",docroot);result=aborting?0:cleanup(root);close(root);return result;}
    if(pending<0)return error("unsafe pending journal",docroot);
    if(pending_mode(pending,mode)<0 || (!aborting && strcmp(mode,requested))){close(pending);return error("pending journal identity or operation mismatch",docroot);}
    root=open_absolute_dir(docroot,1,0755);preserved=auxiliary(docroot,".preserved",0);
    if(root<0 || preserved<0){if(root>=0)close(root);if(preserved>=0)close(preserved);close(pending);return error("preserved bytes unavailable",docroot);}
    /* Validate every pending record and backup before removing any journal record. */
    for(i=0;i<rule_count;i++) {
        status=record_read(pending,i,&size,sha,&perm,&flags);if(status==-2)continue;
        if(status<0){result=error("invalid preservation record",rules[i].path);break;}
        if(aborting || !strcmp(mode,"remove") || (flags&RESTORE_UPGRADE)) {
            if(restore_record(root,preserved,i,size,sha,perm)<0){result=error("cannot restore preserved bytes",rules[i].path);break;}
        } else {
            char path[PATH_MAX],leaf[PATH_MAX],check[65];int p,fd;uint64_t bytes;
            if(snprintf(path,sizeof(path),"%s/%s",sha,rules[i].path)>=(int)sizeof(path)){result=-1;break;}
            p=parent_for(preserved,path,0,leaf);if(p<0){result=-1;break;}fd=regular_at(p,leaf);close(p);
            if(fd<0){result=-1;break;}if(hash_fd(fd,&bytes,check)<0 || bytes!=size || strcmp(check,sha))result=-1;close(fd);if(result)break;
        }
    }
    if(!result && !aborting)result=cleanup(root);
    if(!result) {
        for(i=0;i<rule_count;i++){snprintf(name,sizeof(name),"%zu.record",i);if(unlinkat(pending,name,0)<0 && errno!=ENOENT){result=-1;break;}}
        if(!result && unlinkat(pending,"marker",0)<0)result=-1;
    }
    close(root);close(preserved);close(pending);
    if(!result){char path[PATH_MAX];snprintf(path,sizeof(path),"%s.pending",docroot);if(rmdir(path)<0)result=error("pending directory not empty",path);}
    return result;
}
static int prepare(const char *docroot,const char *mode) {
    int root,preserved,pending,parent,fd,old;char leaf[PATH_MAX],sha[65];uint64_t size;struct stat st;size_t i;
    old=auxiliary(docroot,".pending",0);
    if(old>=0){close(old);if(finish(docroot,mode,1)<0)return -1;}
    else if(old!=-2)return error("unsafe pending directory",docroot);
    root=open_absolute_dir(docroot,1,0755);if(root<0)return error("unsafe document root",docroot);
    /* Preflight all explicit paths. A linked/unreadable managed file blocks the
     * surrounding package operation before its default overwrite/delete actions. */
    for(i=0;i<rule_count;i++) {
        parent=parent_for(root,rules[i].path,0,leaf);if(parent==-2)continue;
        if(parent<0){close(root);return error("unsafe document ancestor",rules[i].path);}
        fd=regular_at(parent,leaf);close(parent);if(fd==-2)continue;
        if(fd<0){close(root);return error("linked or unsupported document",rules[i].path);}
        if(hash_fd(fd,&size,sha)<0){close(fd);close(root);return error("document changed during inspection",rules[i].path);}close(fd);
    }
    preserved=auxiliary(docroot,".preserved",1);pending=auxiliary(docroot,".pending",1);
    if(preserved<0 || pending<0){if(preserved>=0)close(preserved);if(pending>=0)close(pending);close(root);return error("cannot secure preservation directories",docroot);}
    if(marker(pending,1,mode)<0){close(preserved);close(pending);close(root);return error("cannot create preservation journal",docroot);}
    for(i=0;i<rule_count;i++) {
        parent=parent_for(root,rules[i].path,0,leaf);if(parent==-2)continue;
        if(parent<0)goto failed;
        fd=regular_at(parent,leaf);close(parent);if(fd==-2)continue;if(fd<0)goto failed;
        if(hash_fd(fd,&size,sha)<0 || fstat(fd,&st)<0){close(fd);goto failed;}
        if(!rules[i].always && accepted(&rules[i],size,sha)){close(fd);continue;}
        if(snapshot(fd,preserved,&rules[i],size,sha,st.st_mode)<0 || record_write(pending,i,size,sha,st.st_mode,(!rules[i].current?RESTORE_UPGRADE:0))<0){close(fd);goto failed;}
        close(fd);printf("Preserved document copy: %s.preserved/%s/%s\n",docroot,sha,rules[i].path);
    }
    if(fsync(pending)<0)goto failed;
    close(root);close(preserved);close(pending);return 0;
failed:
    close(root);close(preserved);close(pending);return error("preservation incomplete; package changes must not proceed",i<rule_count?rules[i].path:docroot);
}
static int ends_with(const char *s,const char *tail) {size_t a=strlen(s),b=strlen(tail);return a>=b && !strcmp(s+a-b,tail);}
static int maintenance(const char *action,const char *docroot) {
    int root,result;
    if(!strcmp(action,"prepare-upgrade"))return prepare(docroot,"upgrade");
    if(!strcmp(action,"prepare-remove"))return prepare(docroot,"remove");
    if(!strcmp(action,"commit-upgrade"))return finish(docroot,"upgrade",0);
    if(!strcmp(action,"commit-remove"))return finish(docroot,"remove",0);
    if(!strcmp(action,"abort"))return finish(docroot,"",1);
    if(!strcmp(action,"remove-documents")) {
        size_t i;
        if(prepare(docroot,"remove")<0)return -1;
        root=open_absolute_dir(docroot,0,0755);if(root<0)return -1;
        for(i=0;i<rule_count;i++) {
            Rule removal=rules[i];
            if(removal.always)continue;
            removal.retire=1;removal.current=0;
            if(cleanup_one(root,&removal,i)<0){close(root);return -1;}
        }
        close(root);return finish(docroot,"remove",0);
    }
    if(strcmp(action,"cleanup")){errno=EINVAL;return error("unknown action",action);}
    root=open_absolute_dir(docroot,0,0755);if(root<0)return errno==ENOENT?0:error("unsafe document root",docroot);
    result=cleanup(root);close(root);return result;
}
int main(int argc,char **argv) {
    const char *name=strrchr(argv[0],'/'),*action=NULL,*prefix;char docroot[PATH_MAX];int result;
    name=name?name+1:argv[0];
    if(!strcmp(name,"preinstall") || !strcmp(name,"postinstall")) {
        if(argc<4 || !TARGET_MACOS){errno=EINVAL;return error("invalid native package invocation",name)!=0;}
        prefix=argv[3];action=!strcmp(name,"preinstall")?"prepare-upgrade":"commit-upgrade";
        if(snprintf(docroot,sizeof(docroot),"%s/Library/Application Support/SpectralForge/Chimera/Documentation",prefix)>=(int)sizeof(docroot))return 1;
    } else if(ends_with(name,"preinst") || ends_with(name,"postinst") || ends_with(name,"prerm") || ends_with(name,"postrm")) {
        if(argc<2 || TARGET_MACOS)return 1;
        prefix=getenv("DPKG_ROOT");if(!prefix || !*prefix)prefix="/";
        if(snprintf(docroot,sizeof(docroot),"%s/usr/share/doc/chimera-amp-matrix",prefix)>=(int)sizeof(docroot))return 1;
        if(ends_with(name,"preinst") && (!strcmp(argv[1],"install") || !strcmp(argv[1],"upgrade")))action="prepare-upgrade";
        else if(ends_with(name,"postinst") && !strcmp(argv[1],"configure"))action="commit-upgrade";
        else if(ends_with(name,"prerm") && !strcmp(argv[1],"remove"))action="prepare-remove";
        else if(ends_with(name,"postrm") && (!strcmp(argv[1],"remove") || !strcmp(argv[1],"purge")))action="commit-remove";
        else if(!strncmp(argv[1],"abort-",6))action="abort";
        else return 0; /* Never let old prerm/postrm touch a new upgrade journal. */
    } else {
        if(argc!=3){fprintf(stderr,"Usage: document-maintenance ACTION ABSOLUTE-DOCUMENT-ROOT\n");return 2;}
        action=argv[1];if(strlen(argv[2])>=sizeof(docroot))return 2;strcpy(docroot,argv[2]);
    }
    {size_t n=strlen(docroot);while(n>1 && docroot[n-1]=='/')docroot[--n]=0;}
    result=maintenance(action,docroot);return result?1:0;
}
