#include <ntifs.h>
#include <ntddk.h>

NTSTATUS CompleteFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	PKEVENT Event = (PKEVENT)Context;
	if (Event)
		KeSetEvent(Event, 0, 0);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID Unload(PDRIVER_OBJECT DriverObject)
{
	KdPrint(("Unload Success!\n"));
}

VOID DeleteFile(WCHAR *FileName)
{
	OBJECT_ATTRIBUTES	ObjectAttributes;
	UNICODE_STRING		u_FileName;
	PFILE_OBJECT		FileObject;
	PDEVICE_OBJECT		DeviceObject;
	HANDLE				FileHandle;
	NTSTATUS			Status;
	PIRP				Irp;
	PIO_STACK_LOCATION	CurrentLocation;
	KEVENT				Event;
	IO_STATUS_BLOCK		IoBlock;

	ULONG				UnknownValue;

	RtlInitUnicodeString(&u_FileName, FileName);
	ObjectAttributes.Length = 0x18;
	ObjectAttributes.RootDirectory = NULL;
	ObjectAttributes.Attributes = 0x240;
	ObjectAttributes.ObjectName = &u_FileName;
	ObjectAttributes.SecurityDescriptor = NULL;
	ObjectAttributes.SecurityQualityOfService = NULL;
	
	Status = IoCreateFile(&FileHandle, 0, &ObjectAttributes, &IoBlock, 0, 0x80u, 4u, 1u, 0, 0, 0, 0, 0, 0x100u);

	if (!NT_SUCCESS(Status))
	{
		KdPrint(("打开文件失败！ErrorCode is %x\n",Status));
		return;
	}

	Status = ObReferenceObjectByHandle(FileHandle, 0x10000, 0, 0, (PVOID*)&FileObject, 0);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("获取文件对象失败！ErrorCode is %x\n", Status));
		ZwClose(FileHandle);
		return;
	}

	DeviceObject = IoGetBaseFileSystemDeviceObject(FileObject);

	if (DeviceObject == NULL)
	{
		KdPrint(("获取设备对象失败！\n"));
		ZwClose(FileHandle);
		ObfDereferenceObject(FileObject);
		return;
	}

	FileObject->DeleteAccess = 1;

	Irp = IoAllocateIrp(DeviceObject->StackSize,1);
	if (Irp == NULL)
	{
		KdPrint(("分配Irp失败！\n"));
		ZwClose(FileHandle);
		ObfDereferenceObject(FileObject);
		return;
	}

	KeInitializeEvent(&Event, SynchronizationEvent, 0);
	
	UnknownValue = 1;

	Irp->AssociatedIrp.SystemBuffer = &UnknownValue;
	Irp->UserEvent = &Event;
	Irp->UserIosb = &IoBlock;
	Irp->Tail.Overlay.OriginalFileObject = FileObject;
	Irp->Tail.Overlay.Thread = KeGetCurrentThread();
	CurrentLocation = Irp->Tail.Overlay.CurrentStackLocation;
	Irp->RequestorMode = KernelMode;
	--CurrentLocation;
	CurrentLocation->MajorFunction = 6;
	CurrentLocation->DeviceObject = DeviceObject;
	CurrentLocation->FileObject = FileObject;
	CurrentLocation->Parameters.SetFile.FileInformationClass = 13;
	CurrentLocation->Parameters.SetFile.FileObject = FileObject;
	CurrentLocation->Parameters.SetFile.Length = 1;

	CurrentLocation->CompletionRoutine = CompleteFunction;
	CurrentLocation->Context = &Event;
	CurrentLocation->Control = 0xE0;
	Status = IoCallDriver(DeviceObject, Irp);
	if (!NT_SUCCESS(Status))
	{
		KdPrint(("发往设备失败！ErrorCode is %x\n", Status));
		ZwClose(FileHandle);
		ObfDereferenceObject(FileObject);
		return;
	}
	KeWaitForSingleObject(&Event, 0, KernelMode, FALSE, NULL);
	KdPrint(("Irp.status is %x\n", Irp->IoStatus.Status));
	ZwClose(FileHandle);
	ObfDereferenceObject(FileObject);
	return;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegString)
{
	KdPrint(("Entry Driver!\n"));
	DeleteFile(L"\\??\\C:\\Users\\Administrator\\Desktop\\a.txt");
	DriverObject->DriverUnload = Unload;
	return STATUS_SUCCESS;
}