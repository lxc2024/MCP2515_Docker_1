# Use a base image with necessary tools and libraries for building
FROM gcc:latest

# Set the working directory in the container
WORKDIR /app

# Copy the C++ source code into the container
COPY socketcan_example.cpp /app/

# Compile the C++ code
RUN g++ -o socketcan_example socketcan_example.cpp

# Run the executable when the container starts
CMD ["./socketcan_example"]
