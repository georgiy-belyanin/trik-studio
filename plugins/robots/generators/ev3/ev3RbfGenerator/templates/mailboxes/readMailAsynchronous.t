DATA8 @@RANDOM_ID@@
MAILBOX_TEST(@@ID@@, @@RANDOM_ID@@)
JR_NEQ(1,@@RANDOM_ID@@, m1@@RANDOM_ID@@) 
MAILBOX_READ(@@ID@@, @@TYPE_LENGHT@@, 1, @@VARIABLE@@)
m1@@RANDOM_ID@@:
