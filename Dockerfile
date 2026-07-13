FROM alpine:latest

# Install GCC, make, and POSIX threading support
RUN apk add --no-cache gcc musl-dev make

WORKDIR /app

# Copy source files and compile
COPY . .
RUN make

# Expose the server port
EXPOSE 8080

# Default: print usage (override with ./server or ./client at runtime)
CMD echo "Run: docker run -it -p 8080:8080 lanchat ./server" && \
    echo "Or:  docker run -it lanchat ./client"
