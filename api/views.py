from django.http import JsonResponse
from rest_framework import status
from rest_framework.decorators import api_view
from rest_framework.response import Response

from api.serializers import BarcodeVerificationSerializer, VerificationRecordSerializer
from core.models import Driver, VerificationRecord


@api_view(["GET"])
def load_latest_verification_records(request):
	"""
	AJAX endpoint, for loading or getting the latest police verification record.
	Simulates realtime communication.
	"""
	is_ajax = request.META.get('HTTP_X_REQUESTED_WITH') == 'XMLHttpRequest'
	if is_ajax or True:
		latest_records = VerificationRecord.objects.all()[:10]
		return JsonResponse({
			'message': "Updated data successfully.",
			'records': VerificationRecordSerializer(latest_records, many=True).data
		}, status=200)

	return JsonResponse({
		'error': 'Method not allowed.'
	}, status=404)


@api_view(["POST"])
def barcode_verification(request):
	try:
		print("Received Request DATA: ", request.data)
		serializer = BarcodeVerificationSerializer(data=request.data)
		if serializer.is_valid():
			print("Serializer valid: received data from esp32 is okay.....")
			driver = Driver.objects.filter(barcode=serializer.validated_data['barcode']).first()
			if driver:
				VerificationRecord.objects.create(
					driver=driver,
					is_manual=serializer.validated_data['is_manual'],
				)

				if driver.is_permit_valid():
					return Response({
						"details": "Barcode verification successful. Permit valid.",
						"lcd1": f"Permit OK: {driver.format_date()}"[:16],
						"lcd2": f"-> {driver.get_initials()}",
					}, status=status.HTTP_200_OK)

				else:
					return Response({
						"details": "Barcode verification successful. Permit invalid.",
						"lcd1": f"Expired: {driver.format_date()}"[:16],
						"lcd2": f"-> {driver.get_initials()}",
					}, status=status.HTTP_404_NOT_FOUND)

			else:
				return Response({
					'details': f"Driver with barcode {serializer.validated_data['barcode']} not found.",
					'lcd1': "Driver Not Found",
					'lcd2': f"E404: {serializer.validated_data['barcode'][:10]}",
				}, status=status.HTTP_404_NOT_FOUND)

		print("Not valid, ", serializer.errors)
		return Response({
			'error': "Invalid data format. Please check your input.",
			'lcd1': "E400: Data Error....."[:16],
			'lcd2': "Check Input....."[:16]
		}, status=status.HTTP_400_BAD_REQUEST)

	except Exception as e:
		print("Internal Error: ", e)
		return Response({
			'details': f"Internal server error: {e}",
			'lcd1': "Server Error"[:16],
			'lcd2': "Try Again"[:16]
		}, status=status.HTTP_500_INTERNAL_SERVER_ERROR)
