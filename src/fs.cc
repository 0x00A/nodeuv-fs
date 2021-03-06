#include "../fs.h"

namespace nodeuv {

  Buffer& Buffer::operator= (const Buffer &buf) {
    data = buf.data;
    return *this;
  }

  //
  // get the current working directory.
  //
  string Filesystem::cwd() {
    char path[1024];
    size_t size = sizeof(path);
    int err = uv_cwd(path, &size);

    if (err < 0) {
      throw runtime_error(string(uv_err_name(err)));
    }
    return string(path);
  }

  //
  //
  //
  void Filesystem::read(uv_file fd, int64_t bytes, int64_t offset, Callback<Error, uv_buf_t> cb) {

    uv_fs_t read_req;
    uv_buf_t buffer;
    buffer.base = (char *) malloc(bytes);
    buffer.len = bytes;
    Error err;

    static function<void(uv_fs_t* req)> on_read;

    on_read = [&](uv_fs_t* req) {
      auto result = req->result;
      uv_fs_req_cleanup(req);

      if (result > 0) {
        cb(err, buffer);
      }
      else if (result < 0) {
        err.message = uv_err_name(result);
        err.code = result;
        cb(err, buffer);
      }
    };

    uv_fs_read(UV_LOOP, &read_req, fd, &buffer, 1, offset, [](uv_fs_t* req) {
      on_read(req);
    });

    if (!running) {
      uv_run(UV_LOOP, UV_RUN_DEFAULT);
    }
  }


  //
  //
  //
  int Filesystem::readSync(uv_file fd, uv_buf_t* buffer, int64_t offset, int64_t bytes) {

    uv_fs_t read_req;

    int r = uv_fs_read(UV_LOOP, &read_req, fd, buffer, 1, offset, NULL);

    if (!running) {
      uv_run(UV_LOOP, UV_RUN_DEFAULT);
    }

    if (r < 0) {
      auto error = string(uv_err_name(r));
      throw runtime_error("READ: " + error);
    }
    return r;
  } 


  //
  // TODO
  // This should accept length as well
  //
  void Filesystem::write(uv_file fd, uv_buf_t buffer, int64_t offset, Callback<Error> cb) {

    uv_fs_t write_req;
    Error err;

    static function<void(uv_fs_t* req)> on_write;

    on_write = [&](uv_fs_t* req) {
      auto result = req->result;
      uv_fs_req_cleanup(req);

      if (result < 0) {
        err.message = uv_err_name(result);
        err.code = result;
      }
      cb(err);
    };

    uv_fs_write(UV_LOOP, &write_req, fd, &buffer, 1, offset, [](uv_fs_t* req) {
      on_write(req);
    });

    if (!running) {
      uv_run(UV_LOOP, UV_RUN_DEFAULT);
    }
  }


  //
  //
  //
  int Filesystem::writeSync(uv_file fd, Buffer buffer, int64_t offset, int64_t length) {

    uv_fs_t write_req;
    int r = uv_fs_write(UV_LOOP, &write_req, fd, &buffer.data, 1, offset, NULL);

    if (!running) {
      uv_run(UV_LOOP, UV_RUN_DEFAULT);
    }

    if (write_req.result < 0) {
      auto error = string(uv_err_name(write_req.result));
      throw runtime_error("WRITE: " + error);
    }

    return r;
  } 


  //
  //
  //
  void Filesystem::open(string p, int flags, int mode, Callback<Error, uv_file> cb) {

    const char* path = p.c_str();

    uv_fs_t open_req;
    static function<void(uv_fs_t* req)> on_open;
    Error err;

    on_open = [&](uv_fs_t* req) {
      auto result = req->result;
      uv_fs_req_cleanup(req);

      if (result == -1) {
        err.message = uv_err_name(result);
        err.code = result;
      }
      cb(err, result);
    };

    uv_fs_open(UV_LOOP, &open_req, path, flags, mode, [](uv_fs_t* req) {
      on_open(req);
    });

    if (!running) {
      uv_run(UV_LOOP, UV_RUN_DEFAULT);
    }
  }


  //
  //
  //
  int Filesystem::openSync(string p, int flags, int mode) {

    const char* path = p.c_str();

    uv_fs_t open_req;
    int result = uv_fs_open(UV_LOOP, &open_req, path, flags, mode, NULL);
    uv_fs_req_cleanup(&open_req);

    if (!running) {
      uv_run(UV_LOOP, UV_RUN_DEFAULT);
    }

    if (open_req.result < 0) {
      auto subject = string(path);
      auto error = string(uv_err_name(open_req.result));
      throw runtime_error("OPEN: " + error + " \"" + subject + "\"");
    }
    return result;
  };

  //
  //
  //
  void Filesystem::close(uv_file fd, Callback<Error> cb) {
    uv_fs_t close_req;
    static function<void(uv_fs_t* req)> on_close;
    Error err;

    on_close = [&](uv_fs_t* req) {
      int result = req->result;
      if (result == -1) {
        err.message = uv_err_name(result);
        err.code = result;
      }
      cb(err);
    };

    uv_fs_close(UV_LOOP, &close_req, fd, [](uv_fs_t* req) {
      on_close(req);
    });

    if (!running) {
      uv_run(UV_LOOP, UV_RUN_DEFAULT);
    }
  }

  
  //
  //
  //
  int Filesystem::closeSync(uv_file fd) {
    uv_fs_t close_req;
    int r = uv_fs_close(UV_LOOP, &close_req, fd, NULL);

    if (close_req.result < 0) {
      auto error = string(uv_err_name(close_req.result));
      throw runtime_error("CLOSE: " + error);
    }

    return r;
  }


  //
  // Used by both `stat` and `statSync` to build a stats object.
  //
  Stats buildStats(uv_fs_t* req) {
    int result = req->result;
    
    Stats stats;
    if (result >= 0) {

      auto s = static_cast<const uv_stat_t*>(req->ptr);

      stats.dev = s->st_dev;
      stats.mode = s->st_mode;
      stats.nlink = s->st_nlink;
      stats.uid = s->st_uid;
      stats.gid = s->st_gid;
      stats.rdev = s->st_rdev;
      stats.ino = s->st_ino;
      stats.size = s->st_size;
      stats.blksize = s->st_blksize;
      stats.blocks = s->st_blocks;
      stats.flags = s->st_flags;
      stats.gen = s->st_gen;
      stats.atime = s->st_atim;
      stats.mtime = s->st_mtim;
      stats.ctime = s->st_ctim;
      stats.birthtime = s->st_birthtim;
    }

    return stats;
  }

  //
  //
  //
  Stats Filesystem::statSync(string p) {

    const char * path = p.c_str(); 

    uv_fs_t stat_req;
    int r = uv_fs_stat(UV_LOOP, &stat_req, path, NULL);

    if (!running) {
      uv_run(UV_LOOP, UV_RUN_DEFAULT);
    }

    if (stat_req.result < 0) {
      auto subject = string(path);
      auto error = string(uv_err_name(stat_req.result));
      throw runtime_error("STAT: " + error + " \"" + subject + "\"");
    }

    auto stats = buildStats(&stat_req);
    uv_fs_req_cleanup(&stat_req);
    return stats;
  }

  //
  //
  //
  void Filesystem::stat(string p, Callback<Error, Stats> cb) {

    const char* path = p.c_str();

    uv_fs_t stat_req;
    static function<void(uv_fs_t* req)> on_stat;
    Error err;

    on_stat = [&](uv_fs_t* req) {
      int result = req->result;
      auto stats = buildStats(req);
      uv_fs_req_cleanup(req);

      if (result >= 0) {
       cb(err, stats);
      }
      else {
        err.message = uv_err_name(result);
        err.code = result;
        cb(err, stats);
      }
    };

    uv_fs_stat(UV_LOOP, &stat_req, path, [](uv_fs_t* req) {
      on_stat(req);
    });

    if (!running) {
      uv_run(UV_LOOP, UV_RUN_DEFAULT);
    }
  }


  Error Filesystem::mkdirSync(string p) {
    return mkdirSync(p, 0777);
  }


  Error Filesystem::mkdirSync(string p, int mode) {

    const char* path = p.c_str();

    uv_fs_t mkdir_req;
    int result = uv_fs_mkdir(UV_LOOP, &mkdir_req, path, mode, NULL);
    uv_fs_req_cleanup(&mkdir_req);

    if (!running) {
      uv_run(UV_LOOP, UV_RUN_DEFAULT);
    }

    Error err;

    if (result) {
      err.message = uv_err_name(result);
      err.code = result;
    }
    return err;
  }


  Error Filesystem::rmdirSync(string p) {

    const char* path = p.c_str();

    uv_fs_t rmdir_req;
    int result = uv_fs_rmdir(UV_LOOP, &rmdir_req, path, NULL);
    uv_fs_req_cleanup(&rmdir_req);

    if (!running) {
      uv_run(UV_LOOP, UV_RUN_DEFAULT);
    }

    Error err;

    if (result) {
      err.message = uv_err_name(result);
      err.code = result;
    }
    return err;
  }


  //
  //
  //
  Buffer Filesystem::readFileSync(string p) {
    ReadOptions opts;
    return readFileSync(p, opts);
  }

  //
  //
  //
  Buffer Filesystem::readFileSync(string p, ReadOptions opts) {

    const char* path = p.c_str();

    Stats st = statSync(path);
    int fd = openSync(path, opts.flags, opts.mode);
    int size = st.size;

    Buffer buf(size);
    readSync(fd, &buf.data, 0, size);
    closeSync(fd);

    return buf;
  }

  void Filesystem::readFile(string p, ReadOptions opts, Callback<Error, Buffer> cb) {

    const char* path = p.c_str();

    stat(path, [&](auto err, auto stats) {

      Buffer emptyBuffer(0);

      if (err) {
        cb(err, emptyBuffer);
        return;
      }
 
      auto size = stats.size;

      open(path, opts.flags, opts.mode, [&](auto err, auto fd) {

        if (err) {
          cb(err, emptyBuffer);
          return;
        }

        int64_t offset = 0;
        vector<string> bigBuffer;
        int newbuff_size = 0;
        static function<void()> reader;

        reader = [&]() {

          if (offset < size) {
            auto readsize = MAGIC_BUFFER_SIZE;
            auto diff = size - offset;

            if (readsize > diff) {
              readsize = diff;
            }

            read(fd, size, offset, [&](auto err, auto buf) {
              offset = offset + buf.len;

              //
              // this ugly shit creates a copy so we can free
              // buf and move on, this is a shitty way to do
              // this though.
              //
              string s;
              s.assign(buf.base, buf.len);
              newbuff_size += s.size();

              bigBuffer.push_back(s);
              free(buf.base);
              reader();
            });
            return;
          }
          else {
            close(fd, [&](auto err) {

              Buffer buf(newbuff_size);
              int source_start = 0;

              for (auto &b : bigBuffer) {
                buf.copy(Buffer(b), 0, source_start);
                source_start += b.length();
              }

              cb(err, buf);
            });
          }
        }; 

        reader();
      });

    });

  }

  //
  //
  //
  void Filesystem::readFile(string p, Callback<Error, Buffer> cb) {
    ReadOptions opts;
    readFile(p, opts, cb);
  }

  //
  //
  //
  void Filesystem::writeFile(string p, uv_buf_t buffer, WriteOptions opts, Callback<Error> cb) {

    const char* path = p.c_str();

    open(path, opts.flags, opts.mode, [&](auto err, auto fd) {

      if (err) {
        cb(err);
        return;
      }

      write(fd, buffer, 0, [&](auto err) {

        if (err) {
          cb(err);
          return;
        }

        close(fd, cb);

      });
    });
  }


  //
  //
  //
  int Filesystem::writeFileSync(string p, Buffer buffer) {
    WriteOptions opts;
    return writeFileSync(p, buffer, opts);
  }


  //
  //
  //
  int Filesystem::writeFileSync(string p, Buffer buffer, WriteOptions opts) {

    int fd = openSync(p, opts.flags, opts.mode);
    int bytesWritten = writeSync(fd, buffer, 0, buffer.length());
    closeSync(fd);
    return bytesWritten;
  }


  //
  //
  //
  void Filesystem::writeFile(string p, uv_buf_t buffer, Callback<Error> cb) {
    WriteOptions opts;
    writeFile(p, buffer, opts, cb);
  }

  //
  //
  //
  void Filesystem::writeFile(string p, string s, Callback<Error> cb) {
    WriteOptions opts;
    writeFile(p, s, opts, cb);
  }

  //
  //
  //
  void Filesystem::writeFile(string p, string s, WriteOptions opts, Callback<Error> cb) {

    char b[s.size()];
    strcpy(b, s.c_str());
    uv_buf_t buffer;
    buffer = uv_buf_init(b, sizeof(b));
  
    writeFile(p, buffer, opts, cb);  
  }

} // namespace fs

