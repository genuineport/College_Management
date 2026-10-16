#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STUDENTS 500
#define NAME_LENGTH 60
#define GENDER_LENGTH 20
#define COURSE_LENGTH 60
#define DATA_FILE "students.dat"
#define LINE_LENGTH 512

typedef struct {
    int id;
    char name[NAME_LENGTH];
    int age;
    char gender[GENDER_LENGTH];
    char course[COURSE_LENGTH];
    int semester;
    double gpa;
} Student;

static Student students[MAX_STUDENTS];
static int student_count = 0;

static void trim(char *text)
{
    size_t length = strlen(text);
    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
        text[--length] = '\0';
    }
}

static int read_line(const char *prompt, char *buffer, size_t capacity)
{
    int character;

    printf("%s", prompt);
    if (fgets(buffer, (int)capacity, stdin) == NULL) {
        return 0;
    }

    if (strchr(buffer, '\n') == NULL) {
        character = getchar();
        if (character != '\n' && character != EOF) {
            while ((character = getchar()) != '\n' && character != EOF) {
            }
            printf("Input is too long. Please try again.\n");
            return -1;
        }
    }

    trim(buffer);
    return 1;
}

static int read_nonempty_text(const char *prompt, char *buffer, size_t capacity)
{
    int result;
    size_t i;

    for (;;) {
        result = read_line(prompt, buffer, capacity);
        if (result <= 0) {
            if (result == 0) {
                return 0;
            }
            continue;
        }

        if (buffer[0] == '\0') {
            printf("This field cannot be empty.\n");
            continue;
        }

        for (i = 0; buffer[i] != '\0'; ++i) {
            if (buffer[i] == '|') {
                break;
            }
        }
        if (buffer[i] == '|') {
            printf("The | character is not allowed.\n");
            continue;
        }
        return 1;
    }
}

static int read_integer(const char *prompt, int minimum, int maximum, int *value)
{
    char input[80];
    char *end;
    long parsed;
    int result;

    for (;;) {
        result = read_line(prompt, input, sizeof(input));
        if (result == 0) {
            return 0;
        }
        if (result < 0) {
            continue;
        }

        errno = 0;
        parsed = strtol(input, &end, 10);
        while (isspace((unsigned char)*end)) {
            ++end;
        }
        if (input[0] == '\0' || *end != '\0' || errno == ERANGE ||
            parsed < minimum || parsed > maximum) {
            printf("Enter a whole number from %d to %d.\n", minimum, maximum);
            continue;
        }
        *value = (int)parsed;
        return 1;
    }
}

static int read_gpa(double *value)
{
    char input[80];
    char *end;
    double parsed;
    int result;

    for (;;) {
        result = read_line("GPA (0.00-10.00): ", input, sizeof(input));
        if (result == 0) {
            return 0;
        }
        if (result < 0) {
            continue;
        }

        errno = 0;
        parsed = strtod(input, &end);
        while (isspace((unsigned char)*end)) {
            ++end;
        }
        if (input[0] == '\0' || *end != '\0' || errno == ERANGE ||
            !isfinite(parsed) || parsed < 0.0 || parsed > 10.0) {
            printf("Enter a GPA from 0.00 to 10.00.\n");
            continue;
        }
        *value = parsed;
        return 1;
    }
}

static int find_student_index(int id)
{
    int i;
    for (i = 0; i < student_count; ++i) {
        if (students[i].id == id) {
            return i;
        }
    }
    return -1;
}

static int save_students(void)
{
    FILE *file = fopen(DATA_FILE, "w");
    int i;

    if (file == NULL) {
        perror("Could not open student data file for saving");
        return 0;
    }

    for (i = 0; i < student_count; ++i) {
        Student *student = &students[i];
        if (fprintf(file, "%d|%s|%d|%s|%s|%d|%.2f\n",
                    student->id, student->name, student->age,
                    student->gender, student->course,
                    student->semester, student->gpa) < 0) {
            perror("Could not write student data");
            fclose(file);
            return 0;
        }
    }

    if (fclose(file) != 0) {
        perror("Could not finish saving student data");
        return 0;
    }
    return 1;
}

static int load_students(void)
{
    FILE *file = fopen(DATA_FILE, "r");
    char line[LINE_LENGTH];
    int line_number = 0;

    if (file == NULL) {
        if (errno == ENOENT) {
            return 1;
        }
        perror("Could not open student data file");
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        Student student;
        char extra;
        int parsed;
        ++line_number;

        if (strchr(line, '\n') == NULL && !feof(file)) {
            fprintf(stderr, "Student data line %d is too long.\n", line_number);
            fclose(file);
            return 0;
        }

        parsed = sscanf(line, "%d|%59[^|]|%d|%19[^|]|%59[^|]|%d|%lf %c",
                        &student.id, student.name, &student.age,
                        student.gender, student.course, &student.semester,
                        &student.gpa, &extra);
        if (parsed != 7 || student.id <= 0 || student.age < 1 ||
            student.age > 120 || student.semester < 1 ||
            student.semester > 20 || student.gpa < 0.0 ||
            student.gpa > 10.0 || student.name[0] == '\0' ||
            student.gender[0] == '\0' || student.course[0] == '\0' ||
            student_count >= MAX_STUDENTS ||
            find_student_index(student.id) >= 0) {
            fprintf(stderr, "Invalid or duplicate student record at line %d.\n",
                    line_number);
            fclose(file);
            return 0;
        }
        students[student_count++] = student;
    }

    if (ferror(file)) {
        perror("Could not read student data");
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

static int collect_student(Student *student, int allow_id_change, int current_id)
{
    int id;

    for (;;) {
        if (!read_integer("Student ID: ", 1, 2147483647, &id)) {
            return 0;
        }
        if (allow_id_change && id == current_id) {
            break;
        }
        if (find_student_index(id) < 0) {
            break;
        }
        printf("That student ID is already in use.\n");
    }
    student->id = id;

    if (!read_nonempty_text("Full name: ", student->name, sizeof(student->name)) ||
        !read_integer("Age: ", 1, 120, &student->age) ||
        !read_nonempty_text("Gender: ", student->gender, sizeof(student->gender)) ||
        !read_nonempty_text("Course / program: ", student->course, sizeof(student->course)) ||
        !read_integer("Semester: ", 1, 20, &student->semester) ||
        !read_gpa(&student->gpa)) {
        return 0;
    }
    return 1;
}

static void print_student(const Student *student)
{
    printf("%-8d %-24.24s %-5d %-12.12s %-24.24s %-8d %5.2f\n",
           student->id, student->name, student->age, student->gender,
           student->course, student->semester, student->gpa);
}

static void print_table_header(void)
{
    printf("%-8s %-24s %-5s %-12s %-24s %-8s %s\n",
           "ID", "Name", "Age", "Gender", "Course", "Semester", "GPA");
    printf("-----------------------------------------------------------------------------------------------\n");
}

static void add_student(void)
{
    Student student;

    if (student_count >= MAX_STUDENTS) {
        printf("The record limit (%d students) has been reached.\n", MAX_STUDENTS);
        return;
    }
    printf("\n--- Add Student ---\n");
    if (!collect_student(&student, 0, 0)) {
        printf("\nInput ended; returning to the menu.\n");
        return;
    }

    students[student_count++] = student;
    if (save_students()) {
        printf("Student added and saved successfully.\n");
    } else {
        --student_count;
        printf("Student was not added because saving failed.\n");
    }
}

static void list_students(void)
{
    int i;
    if (student_count == 0) {
        printf("\nNo student records found.\n");
        return;
    }

    printf("\n--- Student Records (%d) ---\n", student_count);
    print_table_header();
    for (i = 0; i < student_count; ++i) {
        print_student(&students[i]);
    }
}

static void search_student(void)
{
    int id;
    int index;

    printf("\n--- Search Student ---\n");
    if (!read_integer("Enter student ID: ", 1, 2147483647, &id)) {
        printf("\nInput ended; returning to the menu.\n");
        return;
    }
    index = find_student_index(id);
    if (index < 0) {
        printf("No student found with ID %d.\n", id);
        return;
    }
    print_table_header();
    print_student(&students[index]);
}

static void update_student(void)
{
    int id;
    int index;
    Student original;
    Student updated;

    printf("\n--- Update Student ---\n");
    if (!read_integer("Enter student ID to update: ", 1, 2147483647, &id)) {
        printf("\nInput ended; returning to the menu.\n");
        return;
    }
    index = find_student_index(id);
    if (index < 0) {
        printf("No student found with ID %d.\n", id);
        return;
    }

    printf("Enter the student's updated details:\n");
    if (!collect_student(&updated, 1, id)) {
        printf("\nInput ended; update cancelled.\n");
        return;
    }
    original = students[index];
    students[index] = updated;
    if (save_students()) {
        printf("Student updated and saved successfully.\n");
    } else {
        students[index] = original;
        printf("Saving failed; the student record was restored.\n");
    }
}

static void delete_student(void)
{
    int id;
    int index;
    int i;
    char confirmation[16];
    int result;
    Student removed;

    printf("\n--- Delete Student ---\n");
    if (!read_integer("Enter student ID to delete: ", 1, 2147483647, &id)) {
        printf("\nInput ended; returning to the menu.\n");
        return;
    }
    index = find_student_index(id);
    if (index < 0) {
        printf("No student found with ID %d.\n", id);
        return;
    }

    print_table_header();
    print_student(&students[index]);
    result = read_line("Delete this record? (y/N): ", confirmation, sizeof(confirmation));
    if (result <= 0 || (confirmation[0] != 'y' && confirmation[0] != 'Y')) {
        printf("Delete cancelled.\n");
        return;
    }

    removed = students[index];
    for (i = index; i < student_count - 1; ++i) {
        students[i] = students[i + 1];
    }
    --student_count;
    if (save_students()) {
        printf("Student deleted successfully.\n");
    } else {
        for (i = student_count; i > index; --i) {
            students[i] = students[i - 1];
        }
        students[index] = removed;
        ++student_count;
        printf("Saving failed; the student record was restored.\n");
    }
}

static void show_summary(void)
{
    int i;
    double total_gpa = 0.0;
    int highest_gpa_index = -1;

    printf("\n--- College Summary ---\n");
    printf("Total students: %d\n", student_count);
    if (student_count == 0) {
        return;
    }

    for (i = 0; i < student_count; ++i) {
        total_gpa += students[i].gpa;
        if (highest_gpa_index < 0 ||
            students[i].gpa > students[highest_gpa_index].gpa) {
            highest_gpa_index = i;
        }
    }
    printf("Average GPA: %.2f\n", total_gpa / student_count);
    printf("Highest GPA: %.2f (%s, ID %d)\n",
           students[highest_gpa_index].gpa,
           students[highest_gpa_index].name,
           students[highest_gpa_index].id);
}

static void print_menu(void)
{
    printf("\n========== College Student Management ==========\n");
    printf("1. Add student\n");
    printf("2. List all students\n");
    printf("3. Search student by ID\n");
    printf("4. Update student\n");
    printf("5. Delete student\n");
    printf("6. College summary\n");
    printf("0. Save and exit\n");
}

int main(void)
{
    int choice;

    if (!load_students()) {
        fprintf(stderr, "Could not safely load records. Fix %s before continuing.\n",
                DATA_FILE);
        return EXIT_FAILURE;
    }
    printf("Loaded %d student record(s).\n", student_count);

    for (;;) {
        print_menu();
        if (!read_integer("Choose an option: ", 0, 6, &choice)) {
            printf("\nInput ended; exiting.\n");
            return EXIT_SUCCESS;
        }

        switch (choice) {
        case 1:
            add_student();
            break;
        case 2:
            list_students();
            break;
        case 3:
            search_student();
            break;
        case 4:
            update_student();
            break;
        case 5:
            delete_student();
            break;
        case 6:
            show_summary();
            break;
        case 0:
            if (!save_students()) {
                printf("Could not save records; please resolve the file error before exiting.\n");
                break;
            }
            printf("Records saved. Goodbye.\n");
            return EXIT_SUCCESS;
        }
    }
}
