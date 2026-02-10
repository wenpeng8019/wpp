/**
 * SQTP/1.0 Client Library - XMLHttpRequest Implementation (Callback Style)
 * Structured Query Transfer Protocol JavaScript SDK - Traditional Callback Version
 * 
 * @version 1.0.0
 * @license MIT
 */

(function(global) {
  'use strict';

  /**
   * SQTP Client Constructor
   * @param {string} baseUrl - Database endpoint URL (e.g., 'http://localhost:8080/db/main')
   * @param {object} options - Configuration options
   */
  function SQTP(baseUrl, options) {
    if (!(this instanceof SQTP)) {
      return new SQTP(baseUrl, options);
    }

    this.baseUrl = baseUrl.replace(/\/$/, ''); // Remove trailing slash
    this.options = Object.assign({
      protocol: 'SQTP/1.0',
      timeout: 30000,
      debug: false
    }, options || {});
  }

  /**
   * Internal XHR request wrapper - Traditional Callback Style
   * @private
   * @param {string} method - HTTP method
   * @param {string} fragment - URL fragment
   * @param {object} headers - Request headers
   * @param {*} body - Request body
   * @param {function} callback - Callback function (err, result)
   */
  SQTP.prototype._request = function(method, fragment, headers, body, callback) {
    const self = this;
    const xhr = new XMLHttpRequest();
    const url = fragment ? self.baseUrl + fragment : self.baseUrl;
    
    // Add SQTP- prefix to method
    const sqtpMethod = 'SQTP-' + method;
    
    xhr.open(sqtpMethod, url, true);
    xhr.timeout = self.options.timeout;
    
    // Set headers
    if (headers) {
      Object.keys(headers).forEach(function(key) {
        if (headers[key] !== undefined && headers[key] !== null) {
          xhr.setRequestHeader(key, headers[key]);
        }
      });
    }
    
    // Debug logging
    if (self.options.debug) {
      console.group('%c[SQTP Protocol] ' + method + ' Request', 'color: #007bff; font-weight: bold');
      console.log('%cMethod:', 'color: #28a745; font-weight: bold', method);
      console.log('%cURL:', 'color: #28a745; font-weight: bold', url);
      if (headers && Object.keys(headers).length > 0) {
        console.log('%cHeaders:', 'color: #28a745; font-weight: bold');
        Object.keys(headers).forEach(function(key) {
          if (headers[key] !== undefined && headers[key] !== null) {
            console.log('  ' + key + ':', headers[key]);
          }
        });
      }
      if (body) {
        console.log('%cBody:', 'color: #28a745; font-weight: bold', 
          typeof body === 'string' ? body : JSON.stringify(body, null, 2));
      }
      console.groupEnd();
    }
    
    // Success handler
    xhr.onload = function() {
      const result = {
        status: xhr.status,
        headers: {},
        data: null
      };
      
      // Parse response headers
      const headerStr = xhr.getAllResponseHeaders();
      headerStr.trim().split('\r\n').forEach(function(line) {
        const parts = line.split(': ');
        if (parts.length === 2) {
          result.headers[parts[0]] = parts[1];
        }
      });
      
      // Parse response body
      if (xhr.responseText) {
        try {
          result.data = JSON.parse(xhr.responseText);
        } catch (e) {
          result.data = xhr.responseText;
        }
      }
      
      if (self.options.debug) {
        console.group('%c[SQTP Protocol] ' + method + ' Response', 'color: #17a2b8; font-weight: bold');
        console.log('%cStatus:', 'color: ' + (xhr.status >= 200 && xhr.status < 300 ? '#28a745' : '#dc3545') + '; font-weight: bold', xhr.status);
        if (Object.keys(result.headers).length > 0) {
          console.log('%cResponse Headers:', 'color: #28a745; font-weight: bold');
          Object.keys(result.headers).forEach(function(key) {
            console.log('  ' + key + ':', result.headers[key]);
          });
        }
        if (result.data) {
          console.log('%cResponse Data:', 'color: #28a745; font-weight: bold');
          if (typeof result.data === 'object') {
            if (Array.isArray(result.data)) {
              console.log('  Array with ' + result.data.length + ' items');
              if (result.data.length > 0) {
                console.log('  First item:', result.data[0]);
              }
            } else {
              console.log(result.data);
            }
          } else {
            console.log(result.data);
          }
        }
        console.groupEnd();
      }
      
      if (xhr.status >= 200 && xhr.status < 300) {
        callback(null, result);  // 成功：第一个参数为 null
      } else {
        const error = new Error(result.data?.error || 'Request failed');
        error.status = xhr.status;
        error.response = result;
        callback(error, null);  // 错误：第一个参数为错误对象
      }
    };
    
    // Error handler
    xhr.onerror = function() {
      callback(new Error('Network error'), null);
    };
    
    // Timeout handler
    xhr.ontimeout = function() {
      callback(new Error('Request timeout'), null);
    };
    
    // Send request
    if (body !== undefined && body !== null) {
      xhr.send(typeof body === 'string' ? body : JSON.stringify(body));
    } else {
      xhr.send();
    }
  };

  // ==================== SELECT Query Builder ====================

  function SelectBuilder(client, table) {
    this.client = client;
    this._table = table;
    this._columns = null;
    this._where = [];
    this._whereIn = {};
    this._join = [];
    this._leftJoin = [];
    this._orderBy = null;
    this._limit = null;
    this._offset = null;
    this._distinct = false;
    this._view = 'object';
  }

  SelectBuilder.prototype.columns = function() {
    this._columns = Array.prototype.slice.call(arguments);
    return this;
  };

  SelectBuilder.prototype.where = function(condition) {
    this._where.push(condition);
    return this;
  };

  SelectBuilder.prototype.whereIn = function(column, values) {
    this._whereIn[column] = values;
    return this;
  };

  SelectBuilder.prototype.join = function(table, condition) {
    this._join.push(table + ' ON ' + condition);
    return this;
  };

  SelectBuilder.prototype.leftJoin = function(table, condition) {
    this._leftJoin.push(table + ' ON ' + condition);
    return this;
  };

  SelectBuilder.prototype.orderBy = function(column, direction) {
    this._orderBy = column + ' ' + (direction || 'ASC');
    return this;
  };

  SelectBuilder.prototype.limit = function(count) {
    this._limit = count;
    return this;
  };

  SelectBuilder.prototype.offset = function(count) {
    this._offset = count;
    return this;
  };

  SelectBuilder.prototype.distinct = function() {
    this._distinct = true;
    return this;
  };

  SelectBuilder.prototype.view = function(format) {
    this._view = format;
    return this;
  };

  SelectBuilder.prototype.execute = function(callback) {
    const headers = { 'FROM': this._table };
    let body = null;

    if (this._columns) {
      headers['COLUMNS'] = this._columns.join(', ');
    }
    if (this._where.length > 0) {
      this._where.forEach(function(w, i) {
        if (i === 0) headers['WHERE'] = w;
        else headers['WHERE-' + i] = w;
      });
    }
    if (Object.keys(this._whereIn).length > 0) {
      const keys = Object.keys(this._whereIn);
      if (keys.length === 1) {
        headers['WHERE-IN'] = keys[0];
        body = this._whereIn[keys[0]];
      } else {
        headers['WHERE-IN'] = keys.join(' ');
        body = this._whereIn;
      }
    }
    if (this._join.length > 0) {
      this._join.forEach(function(j, i) {
        headers[i === 0 ? 'JOIN' : 'JOIN-' + i] = j;
      });
    }
    if (this._leftJoin.length > 0) {
      this._leftJoin.forEach(function(j, i) {
        headers[i === 0 ? 'LEFT-JOIN' : 'LEFT-JOIN-' + i] = j;
      });
    }
    if (this._orderBy) headers['ORDER-BY'] = this._orderBy;
    if (this._limit !== null) headers['LIMIT'] = this._limit.toString();
    if (this._offset !== null) headers['OFFSET'] = this._offset.toString();
    if (this._distinct) headers['DISTINCT'] = 'true';
    if (this._view) headers['X-SQTP-VIEW'] = this._view;

    // Convert single-element arrays to strings for WHERE (callback style uses different pattern)
    // No array conversion needed as this implementation uses indexed headers (WHERE-0, WHERE-1)

    this.client._request('SELECT', '', headers, body, callback);
  };

  // ==================== INSERT Query Builder ====================

  function InsertBuilder(client, table) {
    this.client = client;
    this._table = table;
    this._values = null;
    this._ifNotExists = false;
  }

  InsertBuilder.prototype.values = function(data) {
    this._values = data;
    return this;
  };

  InsertBuilder.prototype.ifNotExists = function(flag) {
    this._ifNotExists = flag !== false;
    return this;
  };

  InsertBuilder.prototype.execute = function(callback) {
    const headers = { 'TABLE': this._table };
    if (this._ifNotExists) {
      headers['IF-NOT-EXISTS'] = 'true';
    }
    this.client._request('INSERT', '', headers, this._values, callback);
  };

  // ==================== UPDATE Query Builder ====================

  function UpdateBuilder(client, table) {
    this.client = client;
    this._table = table;
    this._set = null;
    this._where = [];
    this._whereIn = {};
  }

  UpdateBuilder.prototype.set = function(data) {
    this._set = data;
    return this;
  };

  UpdateBuilder.prototype.where = function(condition) {
    this._where.push(condition);
    return this;
  };

  UpdateBuilder.prototype.whereIn = function(column, values) {
    this._whereIn[column] = values;
    return this;
  };

  UpdateBuilder.prototype.execute = function(callback) {
    if (this._where.length === 0 && Object.keys(this._whereIn).length === 0) {
      return callback(new Error('WHERE clause is required for UPDATE. Use where("*") for full table update.'), null);
    }

    const headers = { 'TABLE': this._table };
    const body = Object.assign({}, this._set);

    if (this._where.length > 0) {
      this._where.forEach(function(w, i) {
        headers[i === 0 ? 'WHERE' : 'WHERE-' + i] = w;
      });
    }

    if (Object.keys(this._whereIn).length > 0) {
      const keys = Object.keys(this._whereIn);
      keys.forEach(function(key) {
        headers['WHERE-IN'] = key;
        body[key] = this._whereIn[key];
      }.bind(this));
    }

    this.client._request('UPDATE', '', headers, body, callback);
  };

  // ==================== DELETE Query Builder ====================

  function DeleteBuilder(client, table) {
    this.client = client;
    this._table = table;
    this._where = [];
    this._whereIn = {};
  }

  DeleteBuilder.prototype.where = function(condition) {
    this._where.push(condition);
    return this;
  };

  DeleteBuilder.prototype.whereIn = function(column, values) {
    this._whereIn[column] = values;
    return this;
  };

  DeleteBuilder.prototype.execute = function(callback) {
    if (this._where.length === 0 && Object.keys(this._whereIn).length === 0) {
      return callback(new Error('WHERE clause is required for DELETE. Use where("*") for full table delete.'), null);
    }

    const headers = { 'TABLE': this._table };
    let body = null;

    if (this._where.length > 0) {
      this._where.forEach(function(w, i) {
        headers[i === 0 ? 'WHERE' : 'WHERE-' + i] = w;
      });
    }

    if (Object.keys(this._whereIn).length > 0) {
      const keys = Object.keys(this._whereIn);
      if (keys.length === 1) {
        headers['WHERE-IN'] = keys[0];
        body = this._whereIn[keys[0]];
      } else {
        headers['WHERE-IN'] = keys.join(' ');
        body = this._whereIn;
      }
    }

    this.client._request('DELETE', '', headers, body, callback);
  };

  // ==================== UPSERT Query Builder ====================

  function UpsertBuilder(client, table) {
    this.client = client;
    this._table = table;
    this._key = null;
    this._values = null;
  }

  UpsertBuilder.prototype.key = function(column) {
    this._key = column;
    return this;
  };

  UpsertBuilder.prototype.values = function(data) {
    this._values = data;
    return this;
  };

  UpsertBuilder.prototype.execute = function(callback) {
    const headers = {
      'TABLE': this._table,
      'KEY': this._key
    };
    this.client._request('UPSERT', '', headers, this._values, callback);
  };

  // ==================== CREATE TABLE Builder ====================

  function CreateTableBuilder(client, table) {
    this.client = client;
    this._table = table;
    this._columns = [];
    this._constraints = {};
    this._ifNotExists = false;
  }

  CreateTableBuilder.prototype.column = function(name, type, constraint) {
    this._columns.push({ name: name, type: type, constraint: constraint });
    return this;
  };

  CreateTableBuilder.prototype.primaryKey = function() {
    this._constraints.primaryKey = Array.prototype.slice.call(arguments);
    return this;
  };

  CreateTableBuilder.prototype.unique = function() {
    if (!this._constraints.unique) this._constraints.unique = [];
    this._constraints.unique = this._constraints.unique.concat(Array.prototype.slice.call(arguments));
    return this;
  };

  CreateTableBuilder.prototype.notNull = function() {
    if (!this._constraints.notNull) this._constraints.notNull = [];
    this._constraints.notNull = this._constraints.notNull.concat(Array.prototype.slice.call(arguments));
    return this;
  };

  CreateTableBuilder.prototype.foreignKey = function(constraint) {
    if (!this._constraints.foreignKey) this._constraints.foreignKey = [];
    this._constraints.foreignKey.push(constraint);
    return this;
  };

  CreateTableBuilder.prototype.autoinc = function(column) {
    this._constraints.autoinc = column;
    return this;
  };

  CreateTableBuilder.prototype.ifNotExists = function(flag) {
    this._ifNotExists = flag !== false;
    return this;
  };

  CreateTableBuilder.prototype.execute = function(callback) {
    const headers = { 'NAME': this._table };
    
    if (this._ifNotExists) {
      headers['IF-NOT-EXISTS'] = 'true';
    }

    this._columns.forEach(function(col, i) {
      const key = 'COLUMN' + (i === 0 ? '' : '-' + i);
      headers[key] = col.name + ' ' + col.type + (col.constraint ? ' ' + col.constraint : '');
    });

    if (this._constraints.primaryKey) {
      headers['PRIMARY-KEY'] = this._constraints.primaryKey.join(' ');
    }
    if (this._constraints.unique) {
      headers['UNIQUE'] = this._constraints.unique.join(', ');
    }
    if (this._constraints.notNull) {
      headers['NOT-NULL'] = this._constraints.notNull.join(' ');
    }
    if (this._constraints.foreignKey) {
      this._constraints.foreignKey.forEach(function(fk, i) {
        headers['FOREIGN-KEY' + (i === 0 ? '' : '-' + i)] = fk;
      });
    }
    if (this._constraints.autoinc) {
      headers['AUTOINC'] = this._constraints.autoinc;
    }

    this.client._request('CREATE', '#table', headers, null, callback);
  };

  // ==================== Public API Methods ====================

  SQTP.prototype.select = function(table) {
    return new SelectBuilder(this, table);
  };

  SQTP.prototype.insert = function(table) {
    return new InsertBuilder(this, table);
  };

  SQTP.prototype.update = function(table) {
    return new UpdateBuilder(this, table);
  };

  SQTP.prototype.delete = function(table) {
    return new DeleteBuilder(this, table);
  };

  SQTP.prototype.upsert = function(table) {
    return new UpsertBuilder(this, table);
  };

  SQTP.prototype.createTable = function(table) {
    return new CreateTableBuilder(this, table);
  };

  SQTP.prototype.dropTable = function(table, ifExists, callback) {
    const headers = {
      'NAME': table
    };
    if (ifExists) {
      headers['IF-EXISTS'] = 'true';
    }
    this._request('DROP', '#table', headers, null, callback);
  };

  SQTP.prototype.begin = function(callback) {
    this._request('BEGIN', '', {}, null, callback);
  };

  SQTP.prototype.commit = function(callback) {
    this._request('COMMIT', '', {}, null, callback);
  };

  SQTP.prototype.rollback = function(callback) {
    this._request('ROLLBACK', '', {}, null, callback);
  };

  SQTP.prototype.savepoint = function(name, callback) {
    this._request('SAVEPOINT', '', { 'NAME': name }, null, callback);
  };

  // Export
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = SQTP;
  } else {
    global.SQTP = SQTP;
  }

})(typeof window !== 'undefined' ? window : this);
